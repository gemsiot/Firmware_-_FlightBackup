/*
 * Project Firmware_-_FlightBackup
 * Description:
 * Author:
 * Date:
 */

#include <Particle.h>
#include "dct.h"
#include <Adafruit_SHT4x.h>
#include <PAC1934.h>
#include <PCAL9535A.h>

namespace PinsOB {
	constexpr uint16_t I2C_EXT_EN = 10;
	constexpr uint16_t SD_CD = 8;
	constexpr uint16_t SD_EN = 12;
	constexpr uint16_t AUX_EN = 15;
	constexpr uint16_t CE = 11;
	constexpr uint16_t LED_EN = 13;
	constexpr uint16_t CSA_EN = 14;
	constexpr uint16_t GPS_INT = 7; 
}

PAC1934 csaAlpha(2, 2, 2, 2, 0x18); //CSAs configured for Kestrel v1.6
PAC1934 csaBeta(2, 10, 10, 10, 0x14);

PCAL9535A ioOB(0x20);
// PCAL9535A ioTalon(0x21);

Adafruit_SHT4x atmos;

const String version = "0.1.0";
const String publishLeader = "/secondary/v0";

//For Kestrel v1.6
const int I2C_GLOBAL_EN = D23;
const int I2C_OB_EN = A6;

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

void setup() {
  uint8_t read_value = 0x01; //Set `setup done` bit if not done already
  dct_read_app_data_copy(DCT_SETUP_DONE_OFFSET, &read_value, 1);
  if(read_value != 1)
  {
      const uint8_t write_value = 1;
      dct_write_app_data(&write_value, DCT_SETUP_DONE_OFFSET, 1);
  }

  pinMode(I2C_GLOBAL_EN, OUTPUT);
  digitalWrite(I2C_GLOBAL_EN, LOW); //Disable external I2C

  pinMode(I2C_OB_EN, OUTPUT);
  digitalWrite(I2C_OB_EN, HIGH); //Enable OB I2C

  Wire.begin();
  Serial.begin(9600);
  waitFor(Serial.isConnected, 30000); //Wait up to 30 seconds for serial to connect, otherwise proceed
  Serial.println("Wait for Particle connect - 10m");
  waitFor(Particle.connected, 600000); //Wait up to 10 minutes for particle to connect 
  if(Particle.connected()) Serial.println("Particle Connect: SUCCESS");
  else Serial.println("Particle Connect: FAIL");
  sendMsg("Secondary Firmware Start", "event");
}

void loop() {
  // uint8_t detectedAdrs[128];
  // uint8_t numAdrsDetected = 0;
  String detectedAdrs = "\"ADRS\":["; 
  for(int i = 0; i < 128; i++) {
    Wire.beginTransmission(i);
    int error = Wire.endTransmission();

    // if(error == 0) detectedAdrs[numAdrsDetected++] = i;
    if(error == 0) detectedAdrs = detectedAdrs + String(i) + ","; 
  }
  if(detectedAdrs.endsWith(",")) detectedAdrs.remove(detectedAdrs.length() - 1); //If trailing comma, remove last character
  else detectedAdrs = detectedAdrs + "null"; //Append null if no addresses had been found
  detectedAdrs = detectedAdrs + "]";

  String csaReads = getCSA();
  String atmosRead = getAtmos();

  String report = "{\"Diagnostic\":{" + detectedAdrs + "," + csaReads + "," + atmosRead + ",\"OS\":\"" + System.version() + "\",\"Firm\":\"" + version + "\"}}"; 

  sendMsg(report, "diagnostic");
  delay(60000); //Wait 60 seconds between reports 

}

void sendMsg(String str, String msgType)
{
  String serialMsgType = msgType;
  serialMsgType.toUpperCase(); //Convert to upper case for formatting
  Serial.println(serialMsgType + ":\t" + str);
  if(Particle.connected()) Particle.publish(msgType + publishLeader, str); //Only try to send message via particle if it is connected 
}

String getCSA()
{
  String output = "";
  ioOB.begin();
  ioOB.pinMode(PinsOB::CSA_EN, OUTPUT);
  ioOB.digitalWrite(PinsOB::CSA_EN, HIGH); //Enable CSA GPIO control
  bool initA = csaAlpha.begin();
  bool initB = csaBeta.begin();
  if(initA == true || initB == true) { //Only proceed if one of the ADCs connects correctly
  // adcSense.SetResolution(18); //Set to max resolution (we paid for it right?) 
      //Setup CSAs
      if(initA == true) {
          csaAlpha.enableChannel(Channel::CH1, true); //Enable all channels
          csaAlpha.enableChannel(Channel::CH2, true);
          csaAlpha.enableChannel(Channel::CH3, true);
          csaAlpha.enableChannel(Channel::CH4, true);
          csaAlpha.setCurrentDirection(Channel::CH1, BIDIRECTIONAL);
          csaAlpha.setCurrentDirection(Channel::CH2, UNIDIRECTIONAL);
          csaAlpha.setCurrentDirection(Channel::CH3, UNIDIRECTIONAL);
          csaAlpha.setCurrentDirection(Channel::CH4, UNIDIRECTIONAL);
      }

      if(initB == true) {
          csaBeta.enableChannel(Channel::CH1, true); //Enable all channels
          csaBeta.enableChannel(Channel::CH2, true);
          csaBeta.enableChannel(Channel::CH3, true);
          csaBeta.enableChannel(Channel::CH4, true);
          csaBeta.setCurrentDirection(Channel::CH1, UNIDIRECTIONAL);
          csaBeta.setCurrentDirection(Channel::CH2, UNIDIRECTIONAL);
          csaBeta.setCurrentDirection(Channel::CH3, UNIDIRECTIONAL);
          csaBeta.setCurrentDirection(Channel::CH4, UNIDIRECTIONAL);
      }
output = output + "\"PORT_V\":["; //Open group
// ioSense.digitalWrite(pinsSense::MUX_SEL2, LOW); //Read voltages
      if(initA == true) {
          // csaAlpha.enableChannel(Channel::CH1, true); //Enable all channels
          // csaAlpha.enableChannel(Channel::CH2, true);
          // csaAlpha.enableChannel(Channel::CH3, true);
          // csaAlpha.enableChannel(Channel::CH4, true);
          for(int i = 0; i < 4; i++){ //Increment through all ports
              output = output + String(csaAlpha.getBusVoltage(Channel::CH1 + i, true), 6); //Get bus voltage with averaging 
              output = output + ","; //Append comma 
          }
      }
      else output = output + "null,null,null,null,"; //Append nulls if can't connect to csa alpha

if(initB == true) {
          
          // delay(1000); //Wait for new data //DEBUG!
          for(int i = 0; i < 4; i++){ //Increment through all ports
              output = output + String(csaBeta.getBusVoltage(Channel::CH1 + i, true), 6); //Get bus voltage with averaging 
              if(i < 3) output = output + ","; //Append comma if not the last reading
          }
      }
      else {
          output = output + "null,null,null,null"; //Append nulls if can't connect to csa beta
          // throwError(CSA_INIT_FAIL | 0xB00); //Throw error for ADC beta failure
      }

      output = output + "],"; //Close group
      output = output + "\"PORT_I\":["; //Open group
      if(initA == true) {
          // csaAlpha.enableChannel(Channel::CH1, true); //Enable all channels
          // csaAlpha.enableChannel(Channel::CH2, true);
          // csaAlpha.enableChannel(Channel::CH3, true);
          // csaAlpha.enableChannel(Channel::CH4, true);
          for(int i = 0; i < 4; i++){ //Increment through all ports
              output = output + String(csaAlpha.getCurrent(Channel::CH1 + i, true), 6); //Get bus voltage with averaging 
              output = output + ","; //Append comma 
          }
      }
      else {
          output = output + "null,null,null,null,"; //Append nulls if can't connect to csa alpha
          // throwError(CSA_INIT_FAIL | 0xA00); //Throw error for ADC failure
      }

  if(initB == true) {
          // csaBeta.enableChannel(Channel::CH1, true); //Enable all channels
          // csaBeta.enableChannel(Channel::CH2, true);
          // csaBeta.enableChannel(Channel::CH3, true);
          // csaBeta.enableChannel(Channel::CH4, true);
          for(int i = 0; i < 4; i++){ //Increment through all ports
              output = output + String(csaBeta.getCurrent(Channel::CH1 + i, true), 6); //Get bus voltage with averaging 
              if(i < 3) output = output + ","; //Append comma if not the last reading
          }
      }
      else {
          output = output + "null,null,null,null"; //Append nulls if can't connect to csa beta
          // throwError(CSA_INIT_FAIL | 0xB00); //Throw error for ADC failure
    }
    output = output + "]"; //Close group
}
  else output = "\"PORT_V\":[null],\"PORT_I\":[null]";
  return output;
}

String getAtmos() 
{
  String output = ""; //Used to gather temp from multiple sources
  if(atmos.begin()) {
      atmos.setPrecision(SHT4X_MED_PRECISION); //Set to mid performance 
      sensors_event_t humidity, temp;
      atmos.getEvent(&humidity, &temp);
      output = output + "\"RH\":" + String(humidity.relative_humidity, 4) + ","; //Concatonate atmos data 
      output = output + "\"Temperature\":" + String(temp.temperature, 4);
  }
  else {
      output = output + "\"RH\":null,"; //append null string
      output = output + "\"Temperature\":null";
      //THROW ERROR
  }
  atmos.~Adafruit_SHT4x(); //Delete objects
  return output;
}