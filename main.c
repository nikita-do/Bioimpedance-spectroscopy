/*
 * ad5933_nocal.c
 *
 * Created: 27/05/2022 5:27:30 pm
 * Modified: 13/01/2025 5:27:30 pm
 * Author : DELL
 */
#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include "myusart.h"
#include "mytwi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/*AD5933  registers  addresses*/
#define  real_high_reg  0x94
#define  real_low_reg  0x95
#define  im_high_reg  0x96
#define  im_low_reg  0x97
#define  status_reg  0x8F
#define  control_high_reg  0x80
#define  control_low_reg  0x81

#define  startfreq_reg  0x82
#define  freqinc_reg  0x85
#define  cyclenum_high_reg  0x8A
#define  cyclenum_low_reg  0x8B
#define  incsteps_reg  0x88

#define  AD5933CLK  16776000

/*AD5933  parameters*/
char txBuf[100] = {0};
double Impedance_str[4], Phase_str[4];

/* Input frequencies */
unsigned long int freqrange[4] = {5000, 23000, 9500, 32000};

/*PC app calibrate result with 100000 resistor */
double gainfactor[] = {10055.08178088, 10128.57280923, 10032.93701535, 10191.11753237};
double sys_phase[] = {273.17024303, 285.14823323, 274.94982131, 291.33515967};

/* Supporting functions */
unsigned long int hextodec(unsigned char data_high, unsigned char data_low) {
    unsigned long int data;
    data = (unsigned long int) data_high * 256 + data_low;
    return data;
}

/* Main function */
int main(void) {
    //Declaration  of  variables
    unsigned char* data;
    int Re, Im, j, cnt = 0, Mag;
    unsigned long int i, kl;
    char *val;
    unsigned char real_high, real_low;
    unsigned char im_low, im_high;

    /* Set LED pin as output */
    DDRD = (1 << PD4);
    /* Initializing  USART */
    USART_init();
    /* Initializing  TWI */
    i = TWI_init();

    data = (unsigned char*) malloc(10 * sizeof (unsigned char));
    /*Allocating  memory  for  pointer  which is  used  to  store  data  from  usart*/
    val = (char*) malloc(10 * sizeof (char));

start:
    /* Program start frequency register */
    i = freqrange[cnt] * 32.0023195;
    /* Storing i into data */
    *data = 0x000000ff & (i >> 16);
    *(data + 1) = 0x000000ff & (i >> 8);
    *(data + 2) = 0x000000ff & i;

    TWI_block_write(startfreq_reg, 3, data);
    _delay_ms(200);

    /* number  of  settlings */
    TWI_byte_write(cyclenum_high_reg, 0x00);
    TWI_byte_write(cyclenum_low_reg, 0x64);
    _delay_ms(200);

    //set output voltage:
    j = 1; //j=1: 2Vpp

    /*PLacing  AD5933  in  standby  mode */
    TWI_byte_write(control_high_reg, 0xb1);

    //Program initialize  with  start  frequency command
    TWI_byte_write(control_high_reg, 0x11);

    /*Programming  start  frequency  sweep	and  voltage  range  and  PGA  gain*/
    TWI_byte_write(control_high_reg, 0x21);

    //Waits  until  the  real  and  imaginary  data  in  the  AD5933 is  valid
    while (!(TWI_byte_read(status_reg) & 0x02));

    PORTD = (1 << PD4); // Turn on the LED

    //Reads  the  two  hex  values  from  the  real  register
    real_high = TWI_byte_read(real_high_reg);
    real_low = TWI_byte_read(real_low_reg);
    Re = hextodec(real_high, real_low);
    _delay_ms(10);

    //Reads  the  two  hex  values  from  the  imaginary  register
    im_high = TWI_byte_read(im_high_reg);
    im_low = TWI_byte_read(im_low_reg);
    Im = hextodec(im_high, im_low);
    _delay_ms(10);

    // Calculation
    Mag = sqrt(pow(Re, 2) + pow(Im, 2));
    Impedance_str[cnt] = pow(10, 12) / (gainfactor[cnt] * Mag);
    if (Re > 0 && Im > 0) {
        Phase_str[cnt] = atan(Im / Re) * 57.2957795 - sys_phase[cnt];
    }
    if (Re > 0 && Im < 0) {
        Phase_str[cnt] = 360 + atan(Im / Re) * 57.2957795 - sys_phase[cnt];
    }
    if ((Re < 0 && Im > 0) || (Re < 0 && Im < 0)) {
        Phase_str[cnt] = 180 + atan(Im / Re) * 57.2957795 - sys_phase[cnt];
    }
    
    //Transmit Real, Imaginary
    sprintf(txBuf, "%d", Re);
    USART_CharTransmit(txBuf);
    _delay_ms(100);
    USART_transmit(',');
    sprintf(txBuf, "%d", Im);
    USART_CharTransmit(txBuf);
    _delay_ms(100);
    USART_transmit('\n');

    PORTD = (0 << PD4); // Turn off the LED
    _delay_ms(100);
    
    // Check if four measurements have not finished
    if (cnt < 3) {
        cnt++;
        _delay_ms(100);
        goto start;
    }
    
    else {
        if((Impedance_str[0]/Impedance_str[1])>(Impedance_str[2]/Impedance_str[3]))
        {
            PORTD = (1 << PD4);
            _delay_ms(5000);
            PORTD = (0 << PD4);
        }
        else 
        {
            PORTD = (1 << PD4);
            _delay_ms(500);
            PORTD = (0 << PD4);
            _delay_ms(500);
            PORTD = (1 << PD4);
            _delay_ms(500);
            PORTD = (0 << PD4);
        }
        cnt = 0;
        
        //Power  down  mode
        TWI_byte_write(control_high_reg, 0xA1);
    }
//Setting  up  connection  with  the  PC  interface
com:
    val = USART_CharReceive();
    if ((i = strncmp(val, "z", 1)) == 0) {
        if (val != "") {
            goto start;
        }

    } else {
        goto com;
    }
}