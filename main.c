#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "DHT.h"
#include "lcdpcf8574/lcdpcf8574.h"
#include "avr/interrupt.h"
#include <util/atomic.h>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

#define leftButton PC0
#define rightButton PC2
#define middleButton PC1
#define lightHatcher PB4  //chan D12
#define buzzerPin PB1     //chan D9
#define relayPin PB0      //relay -> D8
#define alarmLight PB2  //den canh bao -> D10

static uint8_t dayTimeToHatch;
static uint8_t hourTimeToHatch;
static uint8_t minTimeToHatch;
static uint8_t tempHatch = 34;
static float currentTemp;
unsigned long currentMillis;
enum State{
	WAITING,
	HATCHING,
	HATCHED,
	CONFIGURING,
	};
static enum State prState;

#define millisWaiting 1000
#define millisHatching 1000
#define millisBuzzer 1000 * 5
#define millisAlarm 1000 * 2

volatile uint32_t millis_count = 0;

void setTimeToHatch();
void setTempHatch();
void printPoint(uint8_t y, uint8_t x);
void updateDisplayTimeToHatch();
void updateDisplayTemp();
void updateDisplay();
void tempAdjusting();
int debounce(uint8_t buttonPin);
void myDigitalWrite(uint8_t pin, uint8_t value);
int8_t myDigitalRead(uint8_t pin);
void lcd_puts_num(uint8_t num);

ISR(TIMER0_COMPA_vect) {
	millis_count++;
}
void millis_init() {
	// Thiết lập Timer0 để tạo ra ngắt mỗi 1ms
	TCCR0A = 0x02;  // CTC mode
	TCCR0B = 0x03;  // Prescaler = 64
	OCR0A = 250;    // (16e6 / 64 / 1000) - 1
	TIMSK0 |= (1 << OCIE0A);
}

uint32_t millis() {
	uint32_t count;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		count = millis_count;
	}
	return count;
}


void setup() {
	sei();
	lcd_init(LCD_DISP_ON);
	lcd_home();
	uint8_t led = 0;
	lcd_led(led);
	// Thiết lập các chân PC0, PC1, và PC2 là đầu vào
	DDRC &= ~(1 << leftButton);
	DDRC &= ~(1 << middleButton);
	DDRC &= ~(1 << rightButton);
	// Đặt các chân PC0, PC1, và PC2 lên mức logic HIGH
	PORTC |= (1 << leftButton) | (1 << middleButton) | (1 << rightButton);

	//Set Led output
	DDRB |= (1 << lightHatcher);
	//Turn Off led
	myDigitalWrite(lightHatcher, LOW);
	
	//Set AlarmLight output
	DDRB |= (1 << alarmLight);
	//Turn Off alarmLight
	myDigitalWrite(alarmLight, LOW);

	//set coi output
	DDRB |= (1 << buzzerPin);
	myDigitalWrite(buzzerPin, LOW);

	//Set out relay
	DDRB |= (1 << relayPin);
	myDigitalWrite(relayPin, LOW);


	//Khoi tao timer
	millis_init();
}
int8_t temperature_int = 0;
int8_t humidity_int = 0;
int main(void) {


	while (1) {
		setup();
		if (dht_GetTempUtil(&temperature_int, &humidity_int) != -1) {
			lcd_clrscr();
			currentTemp = temperature_int;
		}
		setTimeToHatch();
		setTempHatch();
		prState = HATCHING;  // Hatching State

		currentMillis = millis();
		while (prState == HATCHING) {
			if ((millis() - currentMillis) >= millisHatching) {
				currentMillis = millis();
				if (minTimeToHatch == 0)
				if (hourTimeToHatch == 0)
				if (dayTimeToHatch == 0) prState = HATCHED;
				else {
					dayTimeToHatch--;
					hourTimeToHatch = 23;
					minTimeToHatch = 59;
				}
				else {
					hourTimeToHatch--;
					minTimeToHatch = 59;
				}
				else {
					minTimeToHatch--;
				}
			}
			updateDisplayTimeToHatch();
			updateDisplayTemp();
			tempAdjusting();
		}

		// Hatched State
		updateDisplay();

		currentMillis = millis();
		while (prState == HATCHED) {
			while (debounce(middleButton) == 1) {
				if (millis() - currentMillis >= millisAlarm) {
					// Turn on Buzzer 2s
					myDigitalWrite(buzzerPin, HIGH);
					myDigitalWrite(alarmLight, HIGH);
					
					_delay_ms(200);
					myDigitalWrite(buzzerPin, LOW);
					myDigitalWrite(buzzerPin, LOW);

					currentMillis = millis();
				}
			}
			prState = CONFIGURING;
		}
	}
	return 0;
}


void updateDisplayTimeToHatch() {
	lcd_clrscr();
	lcd_gotoxy(8, 0);
	lcd_puts("Config");


	lcd_gotoxy(8, 1);
	if (dayTimeToHatch <= 9) {
		lcd_puts("0");
		lcd_gotoxy(9, 1);
	}
	lcd_puts_num(dayTimeToHatch);
	lcd_gotoxy(10, 1);
	lcd_puts(":");
	lcd_gotoxy(11, 1);
	if (hourTimeToHatch <= 9) {
		lcd_puts("0");
		lcd_gotoxy(12, 1);
	}
	lcd_puts_num(hourTimeToHatch);

	lcd_gotoxy(13, 1);
	lcd_puts(":");
	lcd_gotoxy(14, 1);
	if (minTimeToHatch <= 9) {
		lcd_puts("0");
		lcd_gotoxy(15, 1);
	}
	lcd_puts_num(minTimeToHatch);
	lcd_gotoxy(0, 0);
}
void updateDisplayTemp() {
	lcd_gotoxy(2, 1);
	lcd_puts("|");
	lcd_gotoxy(3, 1);
	lcd_puts_num(currentTemp);
	lcd_gotoxy(0, 0);
	lcd_puts("Temp");
	lcd_gotoxy(0, 1);
	// Kiểm tra nếu đang ở trạng thái cài đặt nhiệt độ
	lcd_puts_num(tempHatch);
}

void updateDisplay() {
	if (prState < CONFIGURING) {
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts("BUONG AP TRUNG");

		lcd_gotoxy(0, 1);
		lcd_puts("TRUNG DA NO!");
	}
}

void tempAdjusting() {
	dht_GetTempUtil(&temperature_int, &humidity_int);
	currentTemp = temperature_int;

	if (currentTemp >= tempHatch + 1) {
		myDigitalWrite(lightHatcher, LOW);
		myDigitalWrite(relayPin, HIGH);
		} else if (currentTemp <= tempHatch - 1) {
		myDigitalWrite(lightHatcher, HIGH);
		myDigitalWrite(relayPin, LOW);
	}
}
int debounce(uint8_t buttonPin) {

	if (!(PINC & (1 << buttonPin))) {
		// Đợi để chắc chắn không phải nhiễu
		_delay_ms(50);
		if (!(PINC & (1 << buttonPin))) {
			return 0;  //da bam
		} else return 1;
	}
	return 1;
}

void setTimeToHatch() {
	dayTimeToHatch = 0;
	hourTimeToHatch = 0;
	minTimeToHatch = 0;
	while (debounce(middleButton) == 1) {
		//	lcd_clrscr();
		updateDisplayTimeToHatch();
		printPoint(8, 1);
		_delay_ms(100);
		if (debounce(leftButton) == 0) {
			if (dayTimeToHatch >= 49) {
				dayTimeToHatch = 49;
				} else {
				dayTimeToHatch++;
			}
			updateDisplayTimeToHatch();
			//_delay_ms(50);
		}

		if (debounce(rightButton) == 0) {
			if (dayTimeToHatch == 0) {
				dayTimeToHatch = 0;
				} else {
				dayTimeToHatch--;
			}
			updateDisplayTimeToHatch();
			//	_delay_ms(50);
		}

		//_delay_ms(100);
	}
	updateDisplayTimeToHatch();
	//_delay_ms(50);

	while (debounce(middleButton) == 1) {
		updateDisplayTimeToHatch();
		printPoint(11, 1);
		_delay_ms(100);

		if (debounce(leftButton) == 0) {
			if (hourTimeToHatch >= 23) {
				hourTimeToHatch = 23;
				} else {
				hourTimeToHatch++;
			}
			updateDisplayTimeToHatch();
			//	_delay_ms(100);
		}

		if (debounce(rightButton) == 0) {
			if (hourTimeToHatch == 0) {
				hourTimeToHatch = 0;
				} else {
				hourTimeToHatch--;
			}
			updateDisplayTimeToHatch();
			//	_delay_ms(50);
		}
		//	_delay_ms(100);
	}
	updateDisplayTimeToHatch();
	//	_delay_ms(50);

	while (debounce(middleButton) == 1) {
		updateDisplayTimeToHatch();
		printPoint(14, 1);
		_delay_ms(100);

		if (debounce(leftButton) == 0) {
			if (minTimeToHatch >= 59) {
				minTimeToHatch = 59;
				} else {
				minTimeToHatch++;
			}
			updateDisplayTimeToHatch();
			//	_delay_ms(100);
		}

		if (debounce(rightButton) == 0) {
			if (minTimeToHatch == 0) {
				minTimeToHatch = 0;
				} else {
				minTimeToHatch--;
			}
			updateDisplayTimeToHatch();
		}
		//	_delay_ms(100);
	}
	updateDisplayTimeToHatch();
	//_delay_ms(50);
}

void setTempHatch() {
	tempHatch = 34;

	while (debounce(middleButton) == 1) {
		updateDisplayTemp();
		printPoint(0, 1);
		_delay_ms(50);
		if (debounce(leftButton) == 0) {
			if (tempHatch >= 60) {
				tempHatch = 60;
				} else {
				tempHatch++;
			}
			updateDisplayTemp();
		}

		if (debounce(rightButton) == 0) {
			if (tempHatch == 10) {
				tempHatch = 10;
				} else {
				tempHatch--;
			}
			updateDisplayTemp();
		}
	}
	updateDisplayTemp();
	_delay_ms(50);
}


void printPoint(uint8_t x, uint8_t y) {
	lcd_gotoxy(x, y);
	lcd_puts("__");
}
int8_t myDigitalRead(uint8_t pin) {
	if (PINB & (1 << pin)) return HIGH;
	else return LOW;
}
void myDigitalWrite(uint8_t pin, uint8_t value) {
	// Kiểm tra giá trị truyền vào
	if (value == LOW) {
		// Thiết lập chân là đầu ra và đặt nó về mức thấp
		PORTB &= ~(1 << pin);
		} else if (value == HIGH) {
		// Thiết lập chân là đầu ra và đặt nó về mức cao
		PORTB |= (1 << pin);
	}
}
void lcd_puts_num(uint8_t num) {
	char buffer[3];         // Buffer to hold the number as a string
	itoa(num, buffer, 10);  // Convert the number to a string
	lcd_puts(buffer);       // Display the string on the LCD
}