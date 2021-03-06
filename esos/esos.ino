//includes
#include "Settings.h"
#include <DallasTemperature.h>
#include <OneWire.h>
#include <dht.h>
#include <RTClib.h>
#include <SD.h>
#include <Seeed_BME280.h>
#include <Wire.h>
#include "log.h"
#include "Clocks.h"
#include "Service.h"
#include "Adafruit_SI1145.h"

//#include "unit.h"

// Factors
const PROGMEM int MIN_WIND_FACTOR = 476;
const PROGMEM int MAX_WIND_FACTOR = 780;

// Procedure
const PROGMEM String GUID_CODE = String(GUID_SLPIOT);


// Dullas Temperature Mesurement
OneWire oneWire(EXTERNAL_TEMP_PIN);
DallasTemperature externalTemp(&oneWire);
DeviceAddress insideThermometer;

// light meter
Adafruit_SI1145 uv = Adafruit_SI1145();
uint8_t is_SI1145_working = 0;

// Clock module
unsigned long lastSendTime;   // last send Time
unsigned long currentTimeToSeconds; // 20/10/18
unsigned long last_ntp_update;
DateTime currentDateTime; // 20/10/2018
String slpLogTime;// 20/10/2018
String istsosLogTime;// 20/10/2018

// dht 11 internal temperature
dht internal_temperature_meter;

// BME 280
BME280 bme280;
uint8_t is_bme280_working = 0;

// global variables
double ext_temperature = 0; // external temperature
double int_temperature = 0; // internal temperature
double int_humidity = 0;  // internal humidity
double ext_humidity = 0;  // external humidity
double soilemoisture_value = 0; // soile mosture
double pressure_value = 0;   // pressure value;
double altitude_value = 0;  // altitude value
double lux_value = 0;       // inetensity value
double water_level =0;
double wind_direction = 0;  // win direction value
// wind speed
double wind_speed = 0;      // wind speed value
float sensor_voltage = 0;
// rain gauge variables
double rain_gauge = 0;
volatile unsigned long rain_count = 0;
volatile unsigned long rain_last = 0;
double battery_value = 0;   // battry value
int loopCount = 0;

void setup() {
  Serial.begin(9600);   // serial monitor for showing
  while (!Serial) {}    // wait for Serial Monitor On
  Serial1.begin(9600);  // serial  for WL
  while (!Serial1) {}   // wait for WL
  Serial2.begin(9600);  // serial  for GPRS
  while (!Serial2) {}   // wait for GPRS Monitor

  //Run Unit tests
#ifdef UNIT_CPP
  unitRun();
#endif

  initialize();

  currentDateTime = getCurruntRTCDate();// 20/10/2018
  currentTimeToSeconds = getUnixTime(currentDateTime); // 20/10/2018
  // set NTP Time
  DateTime tsp = ntpUpdate();
  if (tsp.year() >= 2018) {
    setNTPTime();
  }
  
  // initial sending data,
  readSensorValues();
  saveAndSendData();
  lastSendTime = currentTimeToSeconds;// 20/10/2018
  last_ntp_update = currentTimeToSeconds;// 20/10/2018

}

void loop() {
  currentDateTime = getCurruntRTCDate(); // 20/10/2018
  currentTimeToSeconds = getUnixTime(currentDateTime);// 20/10/2018
  // read sensor values onece
  readSensorValues();
  if ((currentTimeToSeconds - lastSendTime) >= TIME_RATE * 60) {// 20/10/2018
    Serial.println();
    saveAndSendData();
    lastSendTime = currentTimeToSeconds;// 20/10/2018
  }

  if ((currentTimeToSeconds - last_ntp_update) > NTP_UPDATE) {// 20/10/2018
    DateTime tsp = ntpUpdate();
    if (tsp.year() >= 2018) {
      setNTPTime();
    }
    last_ntp_update = currentTimeToSeconds;// 20/10/2018
  }

  if (get_freeRam() < 1000) {
    printSystemLog(F("Reset Program"), F("Ram Refresh"));
    resetProgram();
  }

  // cycle detect
  digitalWrite(CYCLE_CLEAR_PIN,LOW);
  delay(50);
  digitalWrite(CYCLE_CLEAR_PIN,HIGH);
  Serial.print("#");
}

void saveAndSendData() {
  getAvarageSensorValues();

#ifdef ISTSOS
  istsosLogTime = convertTimeToGrinichTimeString(currentDateTime);
  sendIstsosRequest(istsosLogTime);
#endif

#ifdef SLPIOT
  slpLogTime = convertTimeToString(currentDateTime);
  sendSlpiotRequest(slpLogTime);
#endif

  clearSensorVariables();

}


/*
   Get Request String
*/

void sendSlpiotRequest(String time) {
  return sendRequestString(&ext_humidity,
                           &ext_temperature,
                           &int_temperature,
                           &lux_value,
                           &wind_speed,
                           &wind_direction,
                           &rain_gauge,
                           &pressure_value,
                           &soilemoisture_value,
                           &altitude_value,
                           &water_level,
                           &battery_value,
                           SLPIOT_REQUEST,
                           time,
                           GUID_CODE);
}

void sendIstsosRequest(String time) {
  return sendRequestString(&ext_humidity,
                           &ext_temperature,
                           &int_temperature,
                           &lux_value,
                           &wind_speed,
                           &wind_direction,
                           &rain_gauge,
                           &pressure_value,
                           &soilemoisture_value,
                           &altitude_value,
                           &water_level,
                           &battery_value,
                           ISTSOS_REQUEST,
                           time,
                           PROCEDURE);
}

/****************************************************
   SENSOR Reading functions
 * **************************************************
*/

void getAvarageSensorValues() {
  ext_temperature /= loopCount;
  int_temperature /= loopCount;
  int_humidity /= loopCount;
  ext_humidity /= loopCount;
  soilemoisture_value /= loopCount;
  pressure_value /= loopCount;
  pressure_value *= 1000;
  altitude_value /= loopCount;
  lux_value /= loopCount;
  battery_value /= loopCount;
}


void readSensorValues() {
  loopCount++;
  clearLCD();

  String sensorReadingTime = getLocalTime();
  // Watch Dog ON
  Watchdog.enable(WATCHDOG_TIME_OUT);
  // read External temperature
  if (EXT_TEMP_ENABLE) {
    ext_temperature += readExternalTemperature();
    printValuesOnPanel(sensorReadingTime, F("ExTemp"), ext_temperature / loopCount, String((char)223) + "C");
  }
  Watchdog.reset();

  // read Internal temperature
  if (INT_TEMP_ENABLE) {
    int_temperature += readInternalTemperature();
    printValuesOnPanel(sensorReadingTime, F("InTemp"), int_temperature / loopCount, String((char)223) + "C");
  }
  Watchdog.reset();

  // read Internal humidiy
  if (INT_HUM_ENABLE) {
    int_humidity += readInternalHumidity();
    printValuesOnPanel(sensorReadingTime, F("InRH"), int_humidity / loopCount, "%");
  }
  Watchdog.reset();

  // read external humidity
  if (EXT_HUM_ENABLE) {
    ext_humidity += readExternalHumidity();
    printValuesOnPanel(sensorReadingTime, F("RH"), ext_humidity / (loopCount), "%");
  }
  Watchdog.reset();

  // soile mosture value
  if (SM_ENABLE) {
    soilemoisture_value += readSoileMoisture();
    printValuesOnPanel(sensorReadingTime, F("SM"), soilemoisture_value / loopCount, "%");
  }
  Watchdog.reset();

  // pressure value
  if (PRESSURE_ENABLE) {
    pressure_value += readPressure();
    printValuesOnPanel(sensorReadingTime, F("PR"), pressure_value / (loopCount), "kpa");
  }

  Watchdog.reset();

  // altitude value
  if (ALTITUDE_ENABLE) {
    altitude_value += readAltitude();
    printValuesOnPanel(sensorReadingTime, F("AL"), altitude_value / (loopCount), "m");
  }
  Watchdog.reset();

  // lux value
  if (LUX_ENABLE) {
    lux_value += readItensity();
    printValuesOnPanel(sensorReadingTime, F("IN"), lux_value / (loopCount * 1000), "klx");
  }
  Watchdog.reset();

  // wind direction
  if (WD_ENABLE) {
    wind_direction = readWinDirection();
    printValuesOnPanel(sensorReadingTime, F("WD"), wind_direction, String((char)223));
  }
  Watchdog.reset();

  // wind speed
  if (WS_ENABLE) {
    wind_speed = readWindSpeed();
    printValuesOnPanel(sensorReadingTime, F("WS"), wind_speed, "m/s");
  }
  Watchdog.reset();

  // rain guarge
  if (RG_ENABLE) {
    rain_gauge = readRainGuarge();
    printValuesOnPanel(sensorReadingTime, F("RG"), rain_gauge / (loopCount), "mm");
  }
  Watchdog.reset();

  // rain guarge
  if (WL_ENABLE) {
    water_level = readWaterLevel();
    printValuesOnPanel(sensorReadingTime, F("WL"), water_level , "m");
  }
  Watchdog.reset();

  // get battery voltage
  if (BT_ENABLE) {
    battery_value += readBatteryVoltage();
    printValuesOnPanel(sensorReadingTime, F("BT"), battery_value / (loopCount), "V");
  }
  Watchdog.reset();

  // Fan operator
  funcFan();
  Watchdog.reset();
  // station is up
  soundIndicator(0, 1);

  Watchdog.disable();
}

//clear variables setup to sensor Data
void clearSensorVariables() {
  ext_temperature = 0;
  int_temperature = 0;
  ext_humidity = 0;
  int_humidity = 0;
  soilemoisture_value = 0;
  pressure_value = 0;
  altitude_value = 0;
  lux_value = 0;
  wind_direction = 0;
  wind_speed = 0;
  sensor_voltage = 0;
  rain_gauge = 0;
  rain_count = 0;
  water_level =0;
  battery_value = 0;

  loopCount = 0;
}

// read external temperature from dullas
double readExternalTemperature() {
  externalTemp.requestTemperatures();
  if (externalTemp.getTempCByIndex(0) < -120)
  {
    if (!isBME280Working()) {
      printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
      return 0;
    }

    if (bme280.getHumidity() == 0 ) {
      printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
      return 0;
    }
    return bme280.getTemperature();
  }
  return externalTemp.getTempCByIndex(0);
}

//read interna temoerature
double readInternalTemperature() {
  internal_temperature_meter.read11(DHT11_IN_PIN);
  return internal_temperature_meter.temperature;

}

double readInternalHumidity() {
  internal_temperature_meter.read11(DHT11_IN_PIN);
  return internal_temperature_meter.humidity;
}

double readExternalHumidity() {
  if (!isBME280Working()) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }

  if (bme280.getHumidity() == 0 ) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }
  return bme280.getHumidity();
}

// read soile moisture
double readSoileMoisture() {
  double sm = analogRead(SM_PIN);
  if (sm < 1018) {
    sm *= (- 0.0482);
    sm += 49.017;
    return sm;
  } else if (sm < 1018) {
    return 100;
  } else {
    return 0;
  }
}

// read Altitude
double readAltitude() {
  if (!isBME280Working()) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }

  if (bme280.getPressure() > 118000 ) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }
  return bme280.calcAltitude(bme280.getPressure());
}

// read pressure value
double readPressure() {
  if (!isBME280Working()) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }

  if (bme280.getPressure() > 118000 ) {
    printSystemLog(F("I2C ERROR"), F("BME 280"), BME_I2C_ERROR);
    return 0;
  }
  return bme280.getPressure() * 0.001; // kpa
}

// read water level 
// result type = R<v1><v2><v3><v4>\n
double readWaterLevel(){
  char value[4];
  char i=0;
  bool res_starts=false;
  long last = millis();
  double result=0;
  Serial1.flush();
  while(1){
    if(Serial1.available()){
      char c = Serial1.read();
      if(res_starts){
        if(i==4)    // if 4 chars counted , break
          break;
        
        value[i++] = c;  // count and read behind 4 chars
        if(c=='R'){      // Error : if 4 chars contain R , process start again 
          i=0;  
        }
      }
      if(c == 'R'){ // if first charactor R, Counter starts
        res_starts = true;
        continue;
      }
    }
    if ((millis() - last) > 10000UL){ // if read process over 10000ms ,  continue with 0 (If Error on line)
      for(i=0;i<4;i++)
        value[i]='0';
      break;  
    }
  }

  // convert to double
  for(i=0;i<4;i++){
    result += (value[3-i]-48) * pow(10,i); 
  }
  return result /= 1000;  // result as meter value (max 9.999 m)
}

// read lux value
double readItensity() {
//  if (!isSI11450Working()) {
//    printSystemLog(F("I2C ERROR"), F("SI1145"), SI1145_I2C_ERROR);
//    return 0;
//  }

  uint16_t uv_rate = uv.readVisible();
  if (uv_rate < 265)
    return 0;
  else
    return uv_rate ;
}

// read battry values
double readBatteryVoltage() {
  return (analogRead(BATT) * 16.0f / 1023); // 16V ------->[ 4.5V ======>>>> 921  ]  ***** Relevant resistor value 790 ohms

}

// read wind direction
double readWinDirection() {

  sensor_voltage = analogRead(WIN_DIRECTION_PIN) * 0.004882814;  // convert to actual voltage
  if (sensor_voltage <= WIND_DIRECTION_VOLTAGE_MIN && sensor_voltage >= WIND_DIRECTION_VOLTAGE_MAX)
    return 0;
  else {
    return abs(((sensor_voltage - WIND_DIRECTION_VOLTAGE_MIN) * 360 / (WIND_DIRECTION_VOLTAGE_MAX - WIND_DIRECTION_VOLTAGE_MIN))); // convert it to leaniar relationship
  }

}

// read Wind Speed
double readWindSpeed() {
  for (int i = 0; i < 50; i++)
    sensor_voltage += ((float)analogRead(WIN_SPEED_PIN) / 1024.0) * 5.0; // convert to actual voltage

  sensor_voltage /= 50;
  if (sensor_voltage < 0.1)
    return 0;
  else {
    return (sensor_voltage / 4.5) * 32.4; // convert it to leaniar relationship
  }

}

// read rain guarge
double readRainGuarge() {
  return rain_count * RAIN_FACTOR;
}

void rainGageClick()
{
  long thisTime = micros() - rain_last;
  rain_last = micros();
  if (thisTime > 500)
  {
    rain_count++;
  }
}

/*
   SYSTEM INITIALIZATION
*/
void initialize() {
  // one wire intialization
  Wire.begin();
  // LCD
  initLCD();

  // RTC Initialize
  initRTC();
  delay(300);
  // SD init
  initSD();
  delay(300);

  // Cycle clear

  pinMode(CYCLE_CLEAR_PIN,OUTPUT);
  digitalWrite(CYCLE_CLEAR_PIN,HIGH);

  // GPRS
  #if defined(ISTSOS) || defined(SLPIOT)
    ServiceBegin();
    delay(1000);
  
  #endif 

  #ifdef NTP
    if (rtc.lostPower()) {
      setNTPTime();
    }
  #endif

  // Dullas temperature
  if (EXT_TEMP_ENABLE) {
    printString(F("INITIALIZING"), F("DS18B20"));
    externalTemp.begin();
    externalTemp.getAddress(insideThermometer, 0);
    externalTemp.setResolution(insideThermometer, 12);
    printSystemLog(F(SUCCESSFULL), F("DS18B20"));
    delay(1000);
  }

  // BME 280 calibration
  if (EXT_HUM_ENABLE || PRESSURE_ENABLE || ALTITUDE_ENABLE) {
    printString(F("INITIALIZING"), F("BME280"));
    if (!bme280.init()) {
      printSystemLog(F(SUCCESS_ERROR), F("BME280"), BME_NOT_INIT);
      is_bme280_working = 0;
    }
    else {
      printSystemLog(F(SUCCESSFULL), F("BME280"));
      is_bme280_working = 1;
    }
    delay(1000);
  }

  // start light meter
  if (LUX_ENABLE) {
    printString(F("INITIALIZING"), F("SI 1145"));

    if (! uv.begin()) {
      printSystemLog(F(SUCCESS_ERROR), F("SI 1145"), SI1145_NOT_INIT);
      is_SI1145_working = 0;
    } else {
      printSystemLog(F(SUCCESSFULL), F("SI 1145"));
      is_SI1145_working = 1;
    }
  }

  // Rain guarge
  if (RG_ENABLE) {
    pinMode(RAIN_GAUGE_PIN, INPUT);
    digitalWrite(RAIN_GAUGE_PIN, HIGH); // Turn on the internal Pull Up Resistor
    attachInterrupt(RAIN_GAUGE_INT, rainGageClick, FALLING);
  }

  // turn on interrupts
  interrupts();

  // check and initialize fan
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH);

  clearSensorVariables();   // initialize all sensor variables
  printSystemLog(F(SUCCESSFULL), F("SYSTEM INIT"), INIT_DONE);


  delay(2000);

}



// fan on in the range of temperature high
void funcFan() {
  if ((int_temperature / loopCount) > TEMP_UP) {
    digitalWrite(FAN_PIN, LOW);
  }

  if ((int_temperature / loopCount) < TEMP_DOWN) {
    digitalWrite(FAN_PIN, HIGH);
  }
}

// check the bme 280 initialized at the first place.
uint8_t isBME280Working() {
  return is_bme280_working == 1;
}

uint8_t isSI11450Working() {
  return is_SI1145_working == 1;
}

void resetProgram() {
  asm volatile ("  jmp 0");
}

