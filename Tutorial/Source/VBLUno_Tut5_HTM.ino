/*
 * VNGIoTLab VBLUno KIT
 * 
 * SU DUNG CAM BIEN NHIET DO BEN TRONG CHIP nRF51822 DE TAO THIET BI HEATH THERMOMETER (HTM)
 */


#include "nrf_temp.h"
#include <BLE_API.h>

#define DEVICE_NAME       "VBLUno_HTM"
BLE                       ble;

/* Health Thermometer Service */ 
uint8_t             thermTempPayload[5] = { 0, 0, 0, 0, 0 };
GattCharacteristic  thermTemp (GattCharacteristic::UUID_TEMPERATURE_MEASUREMENT_CHAR,thermTempPayload,
                               5, 5, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE);      //biểu thị
GattCharacteristic   *thermChar[] = {&thermTemp};
GattService         thermService (GattService::UUID_HEALTH_THERMOMETER_SERVICE, thermChar, sizeof(thermChar)/sizeof(GattCharacteristic *));

/* Battery Level Service */ /*dịch vụ mức pin*/
uint8_t            batt = 100;     /* Battery Level */ /*mức pin*/
uint8_t            read_batt = 0; /* Variable to hold battery level reads */ /*biến để giữ mức pin đọc*/
GattCharacteristic battLevel   ( GattCharacteristic::UUID_BATTERY_LEVEL_CHAR, (uint8_t *)&batt, 1, 1,
                                 GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |          //Notify or Read /*thông báo hoặc đọc*/
                                 GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ);
GattCharacteristic   *battChar[] = {&battLevel};                                 
GattService        battService ( GattService::UUID_BATTERY_SERVICE, battChar, sizeof(battChar)/sizeof(GattCharacteristic *)); /*characterisc= đặc tính*/

/* Advertising data and parameters */ /*dữ liệu và thông số quảng cáo*/
GapAdvertisingData   advData;
GapAdvertisingData   scanResponse;
GapAdvertisingParams advParams ( GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED ); 
uint16_t             uuid16_list[] = {GattService::UUID_HEALTH_THERMOMETER_SERVICE,
                                      GattService::UUID_BATTERY_SERVICE};
                                      
static volatile bool triggerSensorPolling = false; /* set to high periodically to indicate to the main thread that  /*được đặt thành cao theo định kỳ để cho biết chuỗi chính rằng
                                                                                                                        thăm dò ý kiến là cần thiết*/
                                                    * polling is necessary. */
Ticker tick1s;  /*mã tick1s*/           
unsigned char led_val=LOW;

void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) /*ngắt kết nối*/
  Serial.println("Disconnected!");/* đã ngắt kết nối*/
  Serial.println("Restarting the advertising process"); /*khởi động lại quá trình quảng cáo*/
  ble.startAdvertising();
}

void periodicCallback() {
  if (ble.getGapState().connected) { /*đã kết nối*/
    //blink led //đèn led nháy
    led_val=!led_val;
    digitalWrite(LED, led_val);

    //Thermometer value    // giá trị nhiệt kế 
    uint32_t temp_ieee11073 = quick_ieee11073_from_float(read_temp_value());
    memcpy(thermTempPayload+1, &temp_ieee11073, 4);
    ble.updateCharacteristicValue(thermTemp.getValueAttribute().getHandle(), thermTempPayload, sizeof(thermTempPayload));

    //Batt level value //giá trị mức batt
    /* Decrement the battery level. */ //giảm mức pin 
    batt <=50 ? batt=100 : batt--;
    ble.updateCharacteristicValue(battLevel.getValueAttribute().getHandle(), (uint8_t*)&batt, sizeof(batt));
  }
}
                   
/*
 * brief    Hàm đọc giá trị nhiệt độ từ cảm biến nhiệt độ nội bộ của nRF51822
 * return   Giá trị nhiệt độ (float)
 */
float read_temp_value(){

  float temp=0;

  /* Bắt đầu quá trình đo nhiệt độ. */
  NRF_TEMP->TASKS_START = 1; 

  /* Chờ đến khi quá trình đo nhiệt độ hoàn thành. */
  while (NRF_TEMP->EVENTS_DATARDY == 0){
    // Do nothing.}
  }
  NRF_TEMP->EVENTS_DATARDY = 0;

  /* Đọc giá trị nhiệt độ từ cảm biến
   * Độ chính xác là 0.25 độ C <=> 1 đơn vị = 0.25 độ C
   * Cần chia cho 4 để nhận được giá trị độ C
   */
  temp = ((float)nrf_temp_read() / 4.);

  /* Hoàn thành quá trình đo. */
  NRF_TEMP->TASKS_STOP = 1; 

  return temp;
}

/**
 * @brief A very quick conversion between a float temperature and 11073-20601 FLOAT-Type. // Một chuyển đổi rất nhanh giữa nhiệt độ phao và 11073-20601 FLOAT-Type.
 * @param temperature The temperature as a float. //Nhiệt độ @param Nhiệt độ như một cái phao.
 * @return The temperature in 11073-20601 FLOAT-Type format. // Nhiệt độ ở định dạng FLOAT-Type 11073-20601.
 */
uint32_t quick_ieee11073_from_float(float temperature)
{
    uint8_t  exponent = 0xFE; //exponent is -2
    uint32_t mantissa = (uint32_t)(temperature*100); // nhiệt độ 100 
    return ( ((uint32_t)exponent) << 24) | mantissa; // phần định trị
}
void setup() {
  // put your setup code here, to run once: // đặt mã thiết lập của bạn ở đây, để chạy 1 lần
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  Serial.println("VBLUno - HTM Demo");

  //for Temperature // cho nhiệt độ
  nrf_temp_init();

  //ticker 1 s // mã 1 s
  tick1s.attach(periodicCallback, 1);

  // Init ble
  ble.init();
  ble.onDisconnection(disconnectionCallBack); //ngắt kết nối

  // setup adv_data and srp_data // thiết lập adv_data and srp_data
  ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t*)uuid16_list, sizeof(uuid16_list));
  ble.accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_THERMOMETER);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
  // set adv_type // thiết lập kiểu adv
  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    // add service // thêm dịch vụ
  ble.addService(thermService);
  ble.addService(battService);
  // set device name // đặt tên thiết bị 
  ble.setDeviceName((const uint8_t *)DEVICE_NAME);
  // set tx power,valid values are -40, -20, -16, -12, -8, -4, 0, 4 // đặt tx nguồn các giá trị hợp lệ là -40, -20, -16, -12, -8, -4, 0, 4
  ble.setTxPower(4);
  // set adv_interval, 100ms in multiples of 0.625ms. //đặt adv_interval, 100ms theo bội số của 0,625ms.
  ble.setAdvertisingInterval(160);
  // set adv_timeout, in seconds // thiết lập adv_timeout, tính bằng giâ
  ble.setAdvertisingTimeout(0);
  // start advertising //  bắt đầu quảng cáo
  ble.startAdvertising();
}

void loop() {
  // put your main code here, to run repeatedly: // đặt mã chính của bạn ở đây, để chạy nhiều lần
  ble.waitForEvent();
}
