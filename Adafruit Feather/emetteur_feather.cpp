// Feather9x_RX
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client (receiver)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example Feather9x_TX
 
#include <SPI.h>
#include <RH_RF95.h>
#include <time.h>
//dependance pour capteur DHT11
#include "DHT.h"
//dependances pour capteur I2C MPL115A2
#include <Wire.h>
#include <Adafruit_MPL115A2.h>
//dependance pour le DS18B20
#include <OneWire.h>
/* for feather32u4 */
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
 
/* for feather m0  
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
*/
 
/* for shield 
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 7
*/
 
 
/* for ESP w/featherwing 
#define RFM95_CS  2    // "E"
#define RFM95_RST 16   // "D"
#define RFM95_INT 15   // "B"
*/
 
/* Feather 32u4 w/wing
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     2    // "SDA" (only SDA/SCL/RX/TX have IRQ!)
*/
 
/* Feather m0 w/wing 
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     6    // "D"
*/
 
/* Teensy 3.x w/wing 
#define RFM95_RST     9   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     4    // "C"
*/
 
// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 868.0
 
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);
 
// Blinky on receipt
#define LED 13
#define DHTPIN A1
#define DHTTYPE DHT11
#define photocellPin A0
#define WATER_SENSOR 12
#define HALL_SENSOR A3

DHT dht(DHTPIN,DHTTYPE);
Adafruit_MPL115A2 mpl115a2;
OneWire  ds(10);


// Send 
double temperature;
double humidite;
double numero;
float pressureKPA = 0, temperatureC = 0; 
byte i;
byte present = 0;
byte type_s;
byte Data[12];
byte addr[8];
float celsius, fahrenheit;
int photocellReading;
boolean immerge;
boolean hallstate;

void setup() 
{
  pinMode(LED, OUTPUT);     
  pinMode(RFM95_RST, OUTPUT);
  pinMode(WATER_SENSOR, INPUT);
  pinMode(HALL_SENSOR, INPUT);
  digitalWrite(RFM95_RST, HIGH);
 
  while (!Serial);
  Serial.begin(9600);
  delay(100);
 
  Serial.println("Feather LoRa RX Test!");
  
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
 
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
 
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
 
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
 
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
  srand(time(NULL));
  
  Serial.println("Getting humidity and temperature from DHT11 ...");
  dht.begin();
  
  Serial.println("Getting barometric pressure from Adafruit_MPL115A2 ...");
  mpl115a2.begin();
}
 
void loop()
{
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  digitalWrite(LED, HIGH);
  delay(10);
      
  //DS18B20   
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();

    // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      //Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      //Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      //Serial.println("Device is not a DS18x20 family device.");
      return;
  } 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    Data[i] = ds.read();
  }
  int16_t raw = (Data[1] << 8) | Data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (Data[7] == 0x10) {
      raw = (raw & 0xFFF0) + 12 - Data[6];
    }
  } else {
    byte cfg = (Data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  temperature = dht.readTemperature(false);
  humidite = dht.readHumidity();
  pressureKPA = mpl115a2.getPressure();
  temperatureC = mpl115a2.getTemperature();
  photocellReading = analogRead(photocellPin);
  immerge = isExposedToWater();
  hallstate = isMagnet();
  numero=1;
     
  uint8_t *data;
  data = reinterpret_cast<uint8_t*>(&temperature);
      
  uint8_t *data2;
  data2 = reinterpret_cast<uint8_t*>(&humidite); 

  uint8_t *data3;
  data3 = reinterpret_cast<uint8_t*>(&pressureKPA); 

  uint8_t *data4;
  data4 = reinterpret_cast<uint8_t*>(&temperatureC); 

  uint8_t *data5;
  data5 = reinterpret_cast<uint8_t*>(&celsius);

  uint8_t *data6;
  data6 = reinterpret_cast<uint8_t*>(&photocellReading);

  uint8_t *data7;
  data7 = reinterpret_cast<uint8_t*>(&immerge);

  uint8_t *data8;
  data8 = reinterpret_cast<uint8_t*>(&hallstate);

  uint8_t *data9;
  data9 = reinterpret_cast<uint8_t*>(&numero);

  uint8_t fi[(sizeof(double)*3+sizeof(float)*3+sizeof(int)+sizeof(boolean)*2) / sizeof(uint8_t)];
  int j; 
  for(j=0;j<4;j++){
    fi[j]=data[j];
  }
  for(j=4;j<8;j++){
    fi[j]=data2[j-4];
  }
  for(j=8;j<12;j++){
    fi[j]=data3[j-8];
  }
  for(j=12;j<16;j++){
    fi[j]=data4[j-12];
  }
  for(j=16;j<20;j++){
    fi[j]=data5[j-16];
  }
  for(j=20;j<22;j++){
    fi[j]=data6[j-20];
  }
  for(j=22;j<23;j++){
    fi[j]=data7[j-22];
  }
  for(j=23;j<24;j++){
    fi[j]=data8[j-23];
  }
  for(j=24;j<28;j++){
    fi[j]=data9[j-24];
  }
      
  rf95.send(fi, sizeof(double)*3+sizeof(float)*3+sizeof(int)+sizeof(boolean)*2);
  rf95.waitPacketSent();
  Serial.print("Temperature DHT11 : " );
  Serial.print(temperature);
  Serial.print(" , Humidite : ");
  Serial.print(humidite);
  Serial.print(" , Temperature MPL115A2 : ");
  Serial.print(temperatureC);
  Serial.print(" , Pression atmospherique : ");
  Serial.print(pressureKPA);
  Serial.println();
  Serial.print("Temperature de l'eau = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  Serial.print(photocellReading);     // the raw analog reading
 
  // We'll have a few threshholds, qualitatively determined
  if (photocellReading < 10) {
    Serial.println(" - Dark");
  } else if (photocellReading < 200) {
    Serial.println(" - Dim");
  } else if (photocellReading < 500) {
    Serial.println(" - Light");
  } else if (photocellReading < 800) {
    Serial.println(" - Bright");
  } else {
    Serial.println(" - Very bright");
  }
  Serial.print("Est dans l'eau ? ");
  Serial.print(immerge?"oui | ":"non | ");
  Serial.print("Reception de signal magnetique ? ");
  Serial.println(hallstate?"oui":"non");
  Serial.print("\n");
  digitalWrite(LED, LOW);
  delay(10000);
}

/************************************************************************/
/*Function: Determine whether the sensor is exposed to the water        */
/*Parameter:-void                                                       */
/*Return:   -boolean,if it is exposed to the water,it will return true. */
/************************************************************************/
boolean isExposedToWater()
{
    if(digitalRead(WATER_SENSOR) == LOW){
      Serial.print("Immerge\n");
      return true;
    }
    else{
      Serial.print("Hors de l'eau\n");
      return false;
    }
}

boolean isMagnet()
{
  if(digitalRead(HALL_SENSOR == LOW)){
      Serial.print("signal magnetique\n");
      return true;
  }
  else{
      Serial.print("pas de signal\n");
      return false;
  }
}

