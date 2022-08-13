/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"
int ACK=1; //DEVICE responds or not. if respond,set it to 1. 
#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)
unsigned char* data;//save last two byte of packet
unsigned char* reset_op;//save previous led opcode for reset
//convert 0-F(size=16) to 7 segment code
static unsigned char LED[16]={0xE7,0x06,0xCB,0x8F,0x2E,0xAD,0xED,0x86,0xEF,0xAF,0xEE,0x6D,0xE1,0x4F,0xE9,0xE8};
/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
	unsigned char opcode[2];//opcode sent to device
	switch(packet[0]){
		case MTCP_RESET:
			opcode[0]=MTCP_BIOC_ON;
			opcode[1]=MTCP_LED_USR;
			tuxctl_ldisc_put(tty,opcode,2);//2 is the number of byte in opcode
			tuxctl_ldisc_put(tty,reset_op,6);//6 is the size of led command. show the LED value before reset
			break;
		case MTCP_BIOC_EVENT:
			data[0]=packet[1];
			data[1]=packet[2];
			break;
		case MTCP_ACK:
			ACK=1;//received responce from device
			break;
		default:
			break;

	}

    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{	unsigned char b1,b2,bit5,bit6;//bits for "right left down up C B A START"
	unsigned char opcode[6];//6:max size of opcode
	unsigned char mask;//LED mask
	unsigned char dot;//dot mask
	int i;
    switch (cmd) {
	case TUX_INIT:
		ACK=0;
		opcode[0]=MTCP_BIOC_ON;
		opcode[1]=MTCP_LED_USR;
		tuxctl_ldisc_put(tty,opcode,2);//2 is the number of byte in opcode
		return 0;
	case TUX_BUTTONS:
		//right left down up C B A START
		b1=data[0]&0x0F;//reserve right down left up
		b2=data[1]&0x0F; //reserve C B A START
		b2=b2<<4;    //right shift 4
		bit5=b2&0x20;//0010 0000 get "left" bit
		bit6=b2&0x40;//0100 0000 get "down" bit
		bit5=bit5<<1;//right shift 1
		bit6=bit6>>1;//left shift 1
	
		b2=b2&0x90;//1001 0000 reserve "up"and"right bits
		b2=b2|bit5|bit6|b1;//1.swift left and down.2.combine rdlu and cbas. Now the order is right left down up C B A START
		if (copy_to_user( (unsigned long*)arg, &b2 ,sizeof(b2)) != 0){
			return -EINVAL;//failed
		}
		else{
			return 0;
		}

	case TUX_SET_LED:
		if(ACK==1){//if not waiting
			ACK=0;//force it to wait
			//arg has 4 byte:
			// first byte: showing which dot will show
			//second byte: showing which LED will show
			//third and fourth: the numbers or letters to be shown on four LEDs
			mask=(arg>>16)&0x0F;//get the mask byte
			dot=(arg>>24)&0x0F;//get the dot byte
			opcode[0]=MTCP_LED_SET;
			opcode[1]=0x0F;//1111 represent 4 LEDs
			for(i=0;i<4;i++){//the number of LED
				if(((mask>>i)&0x01)!=0){//if the ith bit of mask is 1, LED i lights on
				opcode[2+i]=LED[arg&0x0F];}//get 7-segment code
				else{
					opcode[2+i]=0x00;//else, set it to 0
				}
				if(((dot>>i)&0x01)!=0){
					opcode[2+i]=opcode[2+i]|0x10;//0x10: the place of dot in 7-segment code
				}
				arg=arg>>4;//4 is the number of bits for 1 led
			}
			for (i=0;i<6;i++){//the size of opcode
				reset_op[i]=opcode[i];//reserve the LED values for reset
			}
			tuxctl_ldisc_put(tty,opcode,6); //6 is the number of byte in opcode
			return 0;
		}
		return 0;

	case TUX_LED_ACK:
		return 0;
	case TUX_LED_REQUEST:
		return 0;
	case TUX_READ_LED:
		return 0;
	default:
	    return -EINVAL;
    }
}

