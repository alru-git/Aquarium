/**
 *******************************
 *
 * Funktionen:
 * 
 *- Temperaturmessung
   * Example sketch showing how to send in DS1820B OneWire temperature readings back to the controller
   * http://www.mysensors.org/build/temp
   * UND: SLEEP() wurde gegen WAIT() getauscht
 * 
 * - Über Switch Funktion kontrolliert, ob der Wasserstand noch ok ist
   * Connect button or door/window reed switch between 
   * digitial I/O pin 2 (da als Interrupt geschaltet) und GND.
 * 
 * - Steuerung des Lichts über IR
   * Sketch zum Versenden der IR Codes 
   * (zum Empfang und Decodieren Original-Mysensor-IR Sketch nehmen: https://www.mysensors.org/build/ir 
   *  oder IR_Decoder)
   *
   *Codes der IR Bedienung vom Aquarium Licht: LED Solar Natur von JBL
   *
   *Protocol: 3 (NEC)
   *Bits: 32
   *
   *on or off: 40BF40BF
   *up:        40BF20DF
   *down:      40BF609F
   *2700k:     40BF30CF
   *4000k:     40BFB04F
   *6700k:     40BF708F
 *
 * Ver. 2.0: Switch wird mit Interrupt aufgerufen
 *           Button dafür auf PIN 2
 *           jetzt auch ohne Bounce2.h, da die Lib mit Interrupt Probleme macht
 *           Schleife für Temp.-Messung alle 5 Min.
 *           heartbeat jetzt wieder in der Loop
 *  
 */


// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24
// MSG1 (Frequenz 2496 MHz)
#define MY_RF24_CHANNEL 96
// MSG2 (Frequenz 2506 MHz)
// #define MY_RF24_CHANNEL 106

// Optional: Define Node ID
#define MY_NODE_ID 100
// Node 0: MSG1 oder MSG2
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

#include <MySensors.h>  

unsigned long WAIT_TIME = 300000;        // 5 Min. Loop Pause

// Switch
  const byte BUTTON = 2;                // PIN 2 ist ein Interrupt PIN
  MyMessage msgS(4,V_TRIPPED);          // Für den Switch wird msgS verwendet
// End Switch

// Temperatur
  #include <DallasTemperature.h>
  #include <OneWire.h>
  #define COMPARE_TEMP 1                // Send temperature only if changed? 1 = Yes 0 = No
  #define ONE_WIRE_BUS 5                // Pin where dallase sensor is connected 
  #define MAX_ATTACHED_DS18B20 16
  OneWire oneWire(ONE_WIRE_BUS);        // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
  DallasTemperature sensors(&oneWire);  // Pass the oneWire reference to Dallas Temperature. 
  float lastTemperature[MAX_ATTACHED_DS18B20];
  int numSensors=0;
  bool receivedConfig = false;
  bool metric = true;
  // Initialize temperature message
  MyMessage msg(0,V_TEMP);
// End Teperatur

// IR
  #include <IRremote.h>                 // für Empf./Send. IR
  #define PinSend 3                     // fest wegen Library
  #define DurPause 500                  // Pause in ms zwischen den Messungen
  String code ="leer";                  // "leer" ist ein Code ohne Wirkung
  IRsend irSend;                        // ? Definition ?  
// End IR

void before()
{
  // Startup up the OneWire library
  sensors.begin();
}

void switchPressed ()
{
// Interrupt Service Routine (ISR)
  int value = digitalRead (BUTTON);
  Serial.print("Status Button: ");
  Serial.println(value);
  send(msgS.set(value==HIGH ? 1 : 0));
}

void setup()  
{ 
  // requestTemperatures() will not block current thread
    sensors.setWaitForConversion(false);

  // Switch
    // internal pull-up resistor
      digitalWrite (BUTTON, HIGH);
    // attach interrupt handler
      attachInterrupt (digitalPinToInterrupt (BUTTON), switchPressed, CHANGE);
    // Initialen Wert senden
      int value = digitalRead (BUTTON);
      Serial.print("Init. Status Button: ");
      Serial.println(value);
      send(msg.set(value==HIGH ? 1 : 0));
  // End Switch

  Serial.println("Setup done...");
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
    sendSketchInfo("Aquarium", "2.0");

  // Fetch the number of attached temperature sensors  
    numSensors = sensors.getDeviceCount();

  // Present all sensors to controller
    // Temperature
    for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {   
     present(i, S_TEMP);
    }

    // Switch
    present(4, S_DOOR);  
    
    // IR
    present(2, S_IR); 
}

void loop() 
{     
  // Update vom heartbeat
  sendHeartbeat();
  
  // Fetch temperatures from Dallas sensors
  sensors.requestTemperatures();

  // query conversion time and wait until conversion completed
  int16_t conversionTime = sensors.millisToWaitForConversion(sensors.getResolution());
  wait (conversionTime);
  
  // Read temperatures and send them to controller 
  for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {
 
    // Fetch and round temperature to one decimal
    float temperature = static_cast<float>(static_cast<int>((getControllerConfig().isMetric?sensors.getTempCByIndex(i):sensors.getTempFByIndex(i)) * 10.)) / 10.;
 
    // Only send data if temperature has changed and no error
    #if COMPARE_TEMP == 1
    if (lastTemperature[i] != temperature && temperature != -127.00 && temperature != 85.00) {
    #else
    if (temperature != -127.00 && temperature != 85.00) {
    #endif
 
      // Send in the new temperature
      send(msg.setSensor(i).set(temperature,1));
      // Save new temperatures for next compare
      lastTemperature[i]=temperature;
      }
    }
   wait (WAIT_TIME);
}
 
void receive(const MyMessage &message) {
    // Empfang der IR Codes und Decodierung
    Serial.print(F("New message: "));
    Serial.println(message.type);

      if (message.type == V_IR_SEND) {
      Serial.print(F("Empfangen: "));
      code = message.getString();
      Serial.println(code);
      }
      
      if ((code == "on") or (code == "off")) {
        irSend.sendNEC(0x40BF40BF, 32);
        Serial.println("Gesendet: on oder off");
        wait (DurPause);
      }
      else if (code == "up") {
        irSend.sendNEC(0x40BF20DF, 32);
        Serial.println("Gesendet: up");
        wait (DurPause);
      }
      else if (code == "down") {
        irSend.sendNEC(0x40BF609F, 32);
        Serial.println("Gesendet: down");
        wait (DurPause);
      }
      if (code == "2700k") {
        irSend.sendNEC(0x40BF30CF, 32);
        Serial.println("Gesendet: 2700k");
        wait (DurPause);
      }
      if (code == "4000k") {
        irSend.sendNEC(0x40BFB04F, 32);
        Serial.println("Gesendet: 4000k");
        wait (DurPause);
      }
      if (code == "6700k") {
        irSend.sendNEC(0x40BF708F, 32);
        Serial.println("Gesendet: 6700k");
        wait (DurPause);
      }
}
