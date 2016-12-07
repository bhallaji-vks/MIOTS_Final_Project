//***********************  main.c  ***********************
// Program written by:
// - Steven Prickett  steven.prickett@gmail.com
//
// Brief desicription of program:
// - Initializes an ESP8266 module to act as a WiFi client
//   and fetch weather data from openweathermap.org
//
//*********************************************************
/* Modified by Jonathan Valvano
 Sept 14, 2016
 Out of the box: to make this work you must
 Step 1) Set parameters of your AP in lines 59-60 of esp8266.c
 Step 2) Change line 39 with directions in lines 40-42
 Step 3) Run a terminal emulator like Putty or TExasDisplay at
         115200 bits/sec, 8 bit, 1 stop, no flow control
 Step 4) Set line 50 to match baud rate of your ESP8266 (9600 or 115200)
 */
/*********************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../inc/tm4c123gh6pm.h"

#include "pll.h"
#include "UART.h"
#include "esp8266.h"
#include "Timer0A.h"
#include "MAX5353.h"
#include "UART2_AudioIn.h"

//#define IP_ADDRESS "172.20.10.11" //MY PI
//#define IP_ADDRESS "184.106.153.149" //Thingspeak
#define IP_ADDRESS "54.86.132.254" //Sparkfun
//#define IP_ADDRESS "52.7.53.111" //Dont Knoq
//#define IP_ADDRESS "192.241.169.168" //Open Weathermap

#define SAMPLE_RATE 44100
#define F441KHz (80000000/SAMPLE_RATE)

// prototypes for functions defined in startup.s
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

//char Fetch[] = "GET /data/2.5/weather?q=Hyderabad&APPID=41b4d86077a170d58a5ec328a4dd8444 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
//char Fetch[] = "GET /data/2.5/weather?q=Boulder%20Colorado&APPID=41b4d86077a170d58a5ec328a4dd8444 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
//char Fetch[] = "GET L\n";
// char Fetch[] = "GET /update?key=CYN49F2F7L1SQ02PL&Temperature=30\r\n";  Read  API
//char Fetch[] = "PUSH url :http://data.sparkfun.com:8080/input/n1n14jAGrvT09dW7lqvQ?private_key=MomoMZGqgyHA4wg7yYrK&data=2\r\n";
//char Fetch[] = "PUSH /data.sparkfun.com/input/n1n14jAGrvT09dW7lqvQ?private_key=MomoMZGqgyHA4wg7yYrK&data=2 HTTP/1.1\r\nHost:api.data.sparkfun.com\r\n\r\n";
char Fetch[] = "PUSH /data.sparkfun.com/input/n1n14jAGrvT09dW7lqvQ?private_key=MomoMZGqgyHA4wg7yYrK&temp=24.48 HTTP/1.1\r\nHost:data.sparkfun.com\r\n\r\n";
//char Fetch[] = "GET /update?key=QPZZTIPEYTH7SLHJ&Temperature=30\r\n"; 
// 1) go to http://openweathermap.org/appid#use 
// 2) Register on the Sign up page
// 3) get an API key (APPID) replace the 1234567890abcdef1234567890abcdef with your APPID

extern bool ESP_Init_Done;

const uint16_t wave[32] = {
  2048*2,2448*2,2832*2,3186*2,3496*2,3751*2,3940*2,4057*2,4095*2,4057*2,3940*2,
  3751*2,3496*2,3186*2,2832*2,2448*2,2048*2,1648*2,1264*2,910*2,600*2,345*2,
  156*2,39*2,0*2,39*2,156*2,345*2,600*2,910*2,1264*2,1648*2};

  
bool Playing = false;
  
void DAC_Out()
{
	char in_data;
	uint16_t out_dataL = 0;
	uint16_t out_dataR = 0;
	//Check if FIFO empty
	if(Rx_DACFifo_Size() > 15000){
		printf("MAKE BUFFER BIGGER");
	}
	//if (Playing){
		if((Rx_DACFifo_Get(&in_data)))
		{	
			//Check for if it's a low byte, then ignore it and grab another
			if((in_data & 0x80 ) == 0){
				//Make sure pop succeeds
				if((Rx_DACFifo_Get(&in_data))){
					//If we got two low bytes in a row = bad news
					if ((in_data & 0x80)==0)
						return;
				}
				else{
					return;
				}
			}
		}
		else{
			return;
		}
		//in_data should now be a high byte
		out_dataL += (in_data & 0x3F)  << 6;
		if(!Rx_DACFifo_Get(&in_data))
				return;
		out_dataL += in_data;
		DAC_Out_left(out_dataL);
		DAC_Out_right(out_dataR);

		/**
		if(!Rx_DACFifo_Get(&in_data))
				return;
		out_dataR += (in_data & 0x3F)  << 6;
		if(!Rx_DACFifo_Get(&in_data))
				return;
		out_dataR += in_data;
		DAC_Out_right(out_dataR);
		if (Rx_DACFifo_Size() < 10){
			Playing = false;
		}
		*/
//	}
//	else{
//		if (Rx_DACFifo_Size() > 8000){
//			Playing = true;
//		}
//	}
		
//	DAC_Out_left(wave[i&0x1F]);
//	DAC_Out_right(wave[i&0x1F]);
}

void GPIO_PF2_Debug_Init(void){
	SYSCTL_RCGCGPIO_R |= 0x20;       // activate port F
	while((SYSCTL_PRGPIO_R&0x0020) == 0){};// ready?
	GPIO_PORTF_DIR_R |= 0x0E;        // make PF3-1 output (PF3-1 built-in LEDs)
	GPIO_PORTF_AFSEL_R &= ~0x0E;     // disable alt funct on PF3-1
	GPIO_PORTF_DEN_R |= 0x0E;        // enable digital I/O on PF3-1
								   // configure PF3-1 as GPIO
	GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF0FF)+0x00000000;
	GPIO_PORTF_AMSEL_R = 0;          // disable analog functionality on PF
}


int main(void){  
//	int i;
//	i = 5;
	DisableInterrupts();
	PLL_Init(Bus80MHz);

	GPIO_PF2_Debug_Init();
//	
	DAC_Init(0x000);
	Timer0A_Init(&DAC_Out,F441KHz);
//	UART2_Init();
//		EnableInterrupts();

//	while(1);
	
	Output_Init();       // UART0 only used for debugging
	
	
    printf("\n\r-----------\n\rSystem starting...\n\r");
	ESP8266_Init(115200);      // connect to access point, set up as client
	ESP8266_GetVersionNumber();
	EnableInterrupts();

	while(1){
		ESP8266_GetStatus();
		if(ESP8266_MakeTCPConnection(IP_ADDRESS))
		//if(ESP8266_MakeTCPConnection("162.243.53.59"))
		{ // open socket in server
			//while(1){
				//if( !ESP_Init_Done)
					ESP8266_SendTCP(Fetch);
			//}
		}
		ESP8266_CloseTCPConnection();
		while(1);
		//for(int k =0; k< 400000; k++);
		//while(Board_Input()==0){// wait for touch};
		//GPIO_PORTF_DATA_R ^= 0x04;
	}
	
}









