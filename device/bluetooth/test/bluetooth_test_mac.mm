// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_mac.h"

#include <stdint.h>

#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#import <CoreBluetooth/CoreBluetooth.h>

using base::mac::ObjCCast;
using base::scoped_nsobject;

namespace device {

// This class hides Objective-C from bluetooth_test_mac.h.
class BluetoothTestMac::ScopedMockCentralManager {
 public:
  explicit ScopedMockCentralManager(MockCentralManager* mock_central_manager) {
    mock_central_manager_.reset(mock_central_manager);
  }

  // Returns MockCentralManager instance.
  MockCentralManager* get() { return mock_central_manager_.get(); };

 private:
  scoped_nsobject<MockCentralManager> mock_central_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMockCentralManager);
};

namespace {

scoped_nsobject<NSDictionary> CreateAdvertisementData(
    NSString* name,
    NSArray* uuids,
    NSDictionary* service_data,
    NSNumber* tx_power) {
  NSMutableDictionary* advertisement_data(
      [NSMutableDictionary dictionaryWithDictionary:@{
        CBAdvertisementDataIsConnectable : @(YES)
      }]);

  if (name) {
    [advertisement_data setObject:name forKey:CBAdvertisementDataLocalNameKey];
  }

  if (uuids) {
    [advertisement_data setObject:uuids
                           forKey:CBAdvertisementDataServiceUUIDsKey];
  }

  if (service_data) {
    [advertisement_data setObject:service_data
                           forKey:CBAdvertisementDataServiceDataKey];
  }

  if (tx_power) {
    [advertisement_data setObject:tx_power
                           forKey:CBAdvertisementDataTxPowerLevelKey];
  }

  return scoped_nsobject<NSDictionary>(advertisement_data,
                                       base::scoped_policy::RETAIN);
}

}  // namespace

// UUID1 hashes to kTestDeviceAddress1, and UUID2 to kTestDeviceAddress2.
const std::string BluetoothTestMac::kTestPeripheralUUID1 =
    "34045B00-0000-0000-0000-000000000000";
const std::string BluetoothTestMac::kTestPeripheralUUID2 =
    "EC1B8F00-0000-0000-0000-000000000000";

BluetoothTestMac::BluetoothTestMac() : BluetoothTestBase() {}

BluetoothTestMac::~BluetoothTestMac() {}

void BluetoothTestMac::SetUp() {}

bool BluetoothTestMac::PlatformSupportsLowEnergy() {
  return BluetoothAdapterMac::IsLowEnergyAvailable();
}

void BluetoothTestMac::InitWithDefaultAdapter() {
  adapter_mac_ = BluetoothAdapterMac::CreateAdapter().get();
  adapter_ = adapter_mac_;
}

void BluetoothTestMac::InitWithoutDefaultAdapter() {
  adapter_mac_ = BluetoothAdapterMac::CreateAdapterForTest(
                     "", "", message_loop_.task_runner())
                     .get();
  adapter_ = adapter_mac_;

  if (BluetoothAdapterMac::IsLowEnergyAvailable()) {
    mock_central_manager_.reset(
        new ScopedMockCentralManager([[MockCentralManager alloc] init]));
    [mock_central_manager_->get() setBluetoothTestMac:this];
    [mock_central_manager_->get() setState:CBCentralManagerStateUnsupported];
    adapter_mac_->SetCentralManagerForTesting((id)mock_central_manager_->get());
  }
}

void BluetoothTestMac::InitWithFakeAdapter() {
  adapter_mac_ =
      BluetoothAdapterMac::CreateAdapterForTest(
          kTestAdapterName, kTestAdapterAddress, message_loop_.task_runner())
          .get();
  adapter_ = adapter_mac_;

  if (BluetoothAdapterMac::IsLowEnergyAvailable()) {
    mock_central_manager_.reset(
        new ScopedMockCentralManager([[MockCentralManager alloc] init]));
    mock_central_manager_->get().bluetoothTestMac = this;
    [mock_central_manager_->get() setState:CBCentralManagerStatePoweredOn];
    adapter_mac_->SetCentralManagerForTesting((id)mock_central_manager_->get());
  }
}

void BluetoothTestMac::ResetEventCounts() {
  BluetoothTestBase::ResetEventCounts();
  last_notify_value_ = false;
}

BluetoothDevice* BluetoothTestMac::SimulateLowEnergyDevice(int device_ordinal) {
  TestBluetoothAdapterObserver observer(adapter_);
  CBCentralManager* central_manager = adapter_mac_->low_energy_central_manager_;
  BluetoothLowEnergyCentralManagerDelegate* central_manager_delegate =
      adapter_mac_->low_energy_central_manager_delegate_;

  const char* identifier;
  NSString* name;
  NSArray* uuids;
  NSNumber* rssi;
  NSDictionary* service_data;
  NSNumber* tx_power;

  switch (device_ordinal) {
    case 1:
      identifier = kTestPeripheralUUID1.c_str();
      name = @(kTestDeviceName.c_str());
      rssi = @(static_cast<int8_t>(TestRSSI::LOWEST));
      uuids = @[
        [CBUUID UUIDWithString:@(kTestUUIDGenericAccess.c_str())],
        [CBUUID UUIDWithString:@(kTestUUIDGenericAttribute.c_str())]
      ];
      service_data = @{
        [CBUUID UUIDWithString:@(kTestUUIDHeartRate.c_str())] :
            [NSData dataWithBytes:(unsigned char[]){1} length:1]
      };
      tx_power = @(static_cast<int8_t>(TestTxPower::LOWEST));
      break;
    case 2:
      identifier = kTestPeripheralUUID1.c_str();
      name = @(kTestDeviceName.c_str());
      rssi = @(static_cast<int8_t>(TestRSSI::LOWER));
      uuids = @[
        [CBUUID UUIDWithString:@(kTestUUIDImmediateAlert.c_str())],
        [CBUUID UUIDWithString:@(kTestUUIDLinkLoss.c_str())]
      ];
      service_data = @{
        [CBUUID UUIDWithString:@(kTestUUIDHeartRate.c_str())] :
            [NSData dataWithBytes:(unsigned char[]){2} length:1],
        [CBUUID UUIDWithString:@(kTestUUIDImmediateAlert.c_str())] :
            [NSData dataWithBytes:(unsigned char[]){0} length:1]
      };
      tx_power = @(static_cast<int8_t>(TestTxPower::LOWER));
      break;
    case 3:
      identifier = kTestPeripheralUUID1.c_str();
      name = @(kTestDeviceNameEmpty.c_str());
      rssi = @(static_cast<int8_t>(TestRSSI::LOW));
      uuids = nil;
      service_data = nil;
      tx_power = nil;
      break;
    case 4:
      identifier = kTestPeripheralUUID2.c_str();
      name = @(kTestDeviceNameEmpty.c_str());
      rssi = @(static_cast<int8_t>(TestRSSI::MEDIUM));
      uuids = nil;
      service_data = nil;
      tx_power = nil;
      break;
    case 5:
      identifier = kTestPeripheralUUID1.c_str();
      name = nil;
      rssi = @(static_cast<int8_t>(TestRSSI::HIGH));
      uuids = nil;
      service_data = nil;
      tx_power = nil;
      break;
    default:
      NOTREACHED() << "SimulateLowEnergyDevice not implemented for "
                   << device_ordinal;
      identifier = nil;
      name = nil;
      rssi = nil;
      uuids = nil;
      service_data = nil;
      tx_power = nil;
  }
  scoped_nsobject<MockCBPeripheral> mock_peripheral([[MockCBPeripheral alloc]
      initWithUTF8StringIdentifier:identifier
                              name:name]);
  mock_peripheral.get().bluetoothTestMac = this;
  [central_manager_delegate
             centralManager:central_manager
      didDiscoverPeripheral:mock_peripheral.get().peripheral
          advertisementData:CreateAdvertisementData(name, uuids, service_data,
                                                    tx_power)
                       RSSI:rssi];
  return observer.last_device();
}

void BluetoothTestMac::SimulateGattConnection(BluetoothDevice* device) {
  BluetoothLowEnergyDeviceMac* lowEnergyDeviceMac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = lowEnergyDeviceMac->GetPeripheral();
  MockCBPeripheral* mockPeripheral = (MockCBPeripheral*)peripheral;
  [mockPeripheral setState:CBPeripheralStateConnected];
  CBCentralManager* centralManager =
      ObjCCast<CBCentralManager>(mock_central_manager_->get());
  [centralManager.delegate centralManager:centralManager
                     didConnectPeripheral:peripheral];
}

void BluetoothTestMac::SimulateConnectedLowEnergyDevice(
    ConnectedDeviceType device_ordinal) {
  const char* identifier = nullptr;
  NSString* name = nil;
  scoped_nsobject<NSMutableSet> cbUUIDs([[NSMutableSet alloc] init]);
  switch (device_ordinal) {
    case ConnectedDeviceType::GENERIC_DEVICE:
      name = @(kTestDeviceName.c_str());
      identifier = kTestPeripheralUUID1.c_str();
      [cbUUIDs
          addObject:[CBUUID UUIDWithString:@(kTestUUIDGenericAccess.c_str())]];
      break;
    case ConnectedDeviceType::HEART_RATE_DEVICE:
      name = @(kTestDeviceName.c_str());
      identifier = kTestPeripheralUUID2.c_str();
      [cbUUIDs
          addObject:[CBUUID UUIDWithString:@(kTestUUIDGenericAccess.c_str())]];
      [cbUUIDs addObject:[CBUUID UUIDWithString:@(kTestUUIDHeartRate.c_str())]];
      break;
  }
  DCHECK(name);
  DCHECK(identifier);
  DCHECK([cbUUIDs.get() count] > 0);
  scoped_nsobject<MockCBPeripheral> mock_peripheral([[MockCBPeripheral alloc]
      initWithUTF8StringIdentifier:identifier
                              name:name]);
  mock_peripheral.get().bluetoothTestMac = this;
  [mock_central_manager_->get()
      setConnectedMockPeripheral:mock_peripheral.get().peripheral
                withServiceUUIDs:cbUUIDs.get()];
}

void BluetoothTestMac::SimulateGattConnectionError(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode errorCode) {
  BluetoothLowEnergyDeviceMac* lowEnergyDeviceMac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = lowEnergyDeviceMac->GetPeripheral();
  MockCBPeripheral* mockPeripheral = (MockCBPeripheral*)peripheral;
  [mockPeripheral setState:CBPeripheralStateDisconnected];
  CBCentralManager* centralManager =
      ObjCCast<CBCentralManager>(mock_central_manager_->get());
  NSError* error =
      BluetoothDeviceMac::GetNSErrorFromConnectErrorCode(errorCode);
  [centralManager.delegate centralManager:centralManager
               didFailToConnectPeripheral:peripheral
                                    error:error];
}

void BluetoothTestMac::SimulateGattDisconnection(BluetoothDevice* device) {
  BluetoothLowEnergyDeviceMac* lowEnergyDeviceMac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = lowEnergyDeviceMac->GetPeripheral();
  MockCBPeripheral* peripheral_mock = (MockCBPeripheral*)peripheral;
  [peripheral_mock setState:CBPeripheralStateDisconnected];
  CBCentralManager* central_manager =
      ObjCCast<CBCentralManager>(mock_central_manager_->get());
  [central_manager.delegate centralManager:central_manager
                   didDisconnectPeripheral:peripheral
                                     error:nil];
}

void BluetoothTestMac::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids) {
  BluetoothLowEnergyDeviceMac* device_mac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = device_mac->GetPeripheral();
  MockCBPeripheral* peripheral_mock = ObjCCast<MockCBPeripheral>(peripheral);
  scoped_nsobject<NSMutableArray> services([[NSMutableArray alloc] init]);
  for (auto uuid : uuids) {
    CBUUID* cb_service_uuid = [CBUUID UUIDWithString:@(uuid.c_str())];
    [services addObject:cb_service_uuid];
  }
  [peripheral_mock addServices:services];
  [peripheral_mock mockDidDiscoverEvents];
}

void BluetoothTestMac::SimulateGattServicesChanged(BluetoothDevice* device) {
  BluetoothLowEnergyDeviceMac* device_mac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = device_mac->GetPeripheral();
  MockCBPeripheral* peripheral_mock = ObjCCast<MockCBPeripheral>(peripheral);
  [peripheral_mock didModifyServices:@[]];
}

void BluetoothTestMac::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  BluetoothUUID bluetooth_service_uuid = service->GetUUID();
  std::string service_uuid_string = bluetooth_service_uuid.canonical_value();
  BluetoothRemoteGattServiceMac* mac_gatt_service =
      static_cast<BluetoothRemoteGattServiceMac*>(service);
  BluetoothDevice* device = service->GetDevice();
  BluetoothLowEnergyDeviceMac* device_mac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* peripheral = device_mac->GetPeripheral();
  MockCBPeripheral* peripheral_mock = ObjCCast<MockCBPeripheral>(peripheral);
  [peripheral_mock removeService:mac_gatt_service->GetService()];
  // After -[MockCBPeripheral removeService:], BluetoothLowEnergyDeviceMac is
  // expected to call -[CBPeripheral discoverServices:]
  [peripheral_mock mockDidDiscoverEvents];
}

void BluetoothTestMac::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  BluetoothRemoteGattServiceMac* mac_gatt_service =
      static_cast<BluetoothRemoteGattServiceMac*>(service);
  CBService* cb_service = mac_gatt_service->GetService();
  MockCBService* service_mock = ObjCCast<MockCBService>(cb_service);
  CBUUID* cb_uuid = [CBUUID UUIDWithString:@(uuid.c_str())];
  [service_mock addCharacteristicWithUUID:cb_uuid properties:properties];
  MockCBPeripheral* peripheral_mock = GetMockCBPeripheral(service);
  [peripheral_mock didModifyServices:@[]];
  // After -[MockCBPeripheral didModifyServices:], BluetoothLowEnergyDeviceMac
  // is expected to call -[CBPeripheral discoverServices:]
  [peripheral_mock mockDidDiscoverEvents];
}

void BluetoothTestMac::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  scoped_nsobject<NSData> data(
      [[NSData alloc] initWithBytes:value.data() length:value.size()]);
  [characteristic_mock simulateReadWithValue:data error:nil];
}

void BluetoothTestMac::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  NSError* error = BluetoothDeviceMac::GetNSErrorFromGattErrorCode(error_code);
  [characteristic_mock simulateReadWithValue:nil error:error];
}

void BluetoothTestMac::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  [characteristic_mock simulateWriteWithError:nil];
}

void BluetoothTestMac::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  NSError* error = BluetoothDeviceMac::GetNSErrorFromGattErrorCode(error_code);
  [characteristic_mock simulateWriteWithError:error];
}

void BluetoothTestMac::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  CBUUID* cb_uuid = [CBUUID UUIDWithString:@(uuid.c_str())];
  [characteristic_mock simulateDescriptorWithUUID:cb_uuid];
  MockCBPeripheral* peripheral_mock = GetMockCBPeripheral(characteristic);
  [peripheral_mock didModifyServices:@[]];
  // After -[MockCBPeripheral didModifyServices:], BluetoothLowEnergyDeviceMac
  // is expected to call -[CBPeripheral discoverServices:]
  [peripheral_mock mockDidDiscoverEvents];
}

void BluetoothTestMac::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  [characteristic_mock simulateGattNotifySessionStarted];
}

void BluetoothTestMac::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  NSError* error = BluetoothDeviceMac::GetNSErrorFromGattErrorCode(error_code);
  [characteristic_mock simulateGattNotifySessionFailedWithError:error];
}

void BluetoothTestMac::SimulateGattNotifySessionStopped(
    BluetoothRemoteGattCharacteristic* characteristic) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  [characteristic_mock simulateGattNotifySessionStopped];
}

void BluetoothTestMac::SimulateGattNotifySessionStopError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  NSError* error = BluetoothDeviceMac::GetNSErrorFromGattErrorCode(error_code);
  [characteristic_mock simulateGattNotifySessionStoppedWithError:error];
}

void BluetoothTestMac::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  MockCBCharacteristic* characteristic_mock =
      GetCBMockCharacteristic(characteristic);
  scoped_nsobject<NSData> data(
      [[NSData alloc] initWithBytes:value.data() length:value.size()]);
  [characteristic_mock simulateGattCharacteristicChangedWithValue:data];
}

void BluetoothTestMac::SimulateGattCharacteristicRemoved(
    BluetoothRemoteGattService* service,
    BluetoothRemoteGattCharacteristic* characteristic) {
  MockCBPeripheral* peripheral_mock = GetMockCBPeripheral(service);
  BluetoothRemoteGattServiceMac* mac_gatt_service =
      static_cast<BluetoothRemoteGattServiceMac*>(service);
  CBService* cb_service = mac_gatt_service->GetService();
  MockCBService* service_mock = ObjCCast<MockCBService>(cb_service);
  BluetoothRemoteGattCharacteristicMac* characteristic_mac =
      static_cast<BluetoothRemoteGattCharacteristicMac*>(characteristic);
  CBCharacteristic* cb_characteristic =
      characteristic_mac->GetCBCharacteristic();
  MockCBCharacteristic* characteristic_mock =
      ObjCCast<MockCBCharacteristic>(cb_characteristic);
  [service_mock removeCharacteristicMock:characteristic_mock];
  [peripheral_mock mockDidDiscoverEvents];
}

void BluetoothTestMac::ExpectedChangeNotifyValueAttempts(int attempts) {
  EXPECT_EQ(attempts, gatt_notify_characteristic_attempts_);
}

void BluetoothTestMac::ExpectedNotifyValue(
    NotifyValueState expected_value_state) {
  switch (expected_value_state) {
    case NotifyValueState::NONE:
      EXPECT_FALSE(last_notify_value_);
      break;
    case NotifyValueState::NOTIFY:
    case NotifyValueState::INDICATE:
      EXPECT_TRUE(last_notify_value_);
      break;
  }
}

void BluetoothTestMac::OnFakeBluetoothDeviceConnectGattCalled() {
  gatt_connection_attempts_++;
}

void BluetoothTestMac::OnFakeBluetoothGattDisconnect() {
  gatt_disconnection_attempts_++;
}

void BluetoothTestMac::OnFakeBluetoothServiceDiscovery() {
  gatt_discovery_attempts_++;
}

void BluetoothTest::OnFakeBluetoothCharacteristicReadValue() {
  gatt_read_characteristic_attempts_++;
}

void BluetoothTest::OnFakeBluetoothCharacteristicWriteValue(
    std::vector<uint8_t> value) {
  last_write_value_ = value;
  gatt_write_characteristic_attempts_++;
}

void BluetoothTest::OnFakeBluetoothGattSetCharacteristicNotification(
    bool notify_value) {
  last_notify_value_ = notify_value;
  gatt_notify_characteristic_attempts_++;
}

BluetoothDevice::UUIDSet
BluetoothTestMac::RetrieveConnectedPeripheralServiceUUIDs() {
  BluetoothDevice::UUIDSet service_uuids;
  for (CBUUID* uuid in
       [mock_central_manager_->get() retrieveConnectedPeripheralServiceUUIDs]) {
    service_uuids.insert(BluetoothAdapterMac::BluetoothUUIDWithCBUUID(uuid));
  }
  return service_uuids;
}

void BluetoothTestMac::ResetRetrieveConnectedPeripheralServiceUUIDs() {
  [mock_central_manager_->get() resetRetrieveConnectedPeripheralServiceUUIDs];
}

MockCBPeripheral* BluetoothTestMac::GetMockCBPeripheral(
    BluetoothRemoteGattService* service) const {
  BluetoothDevice* device = service->GetDevice();
  BluetoothLowEnergyDeviceMac* device_mac =
      static_cast<BluetoothLowEnergyDeviceMac*>(device);
  CBPeripheral* cb_peripheral = device_mac->GetPeripheral();
  return ObjCCast<MockCBPeripheral>(cb_peripheral);
}

MockCBPeripheral* BluetoothTestMac::GetMockCBPeripheral(
    BluetoothRemoteGattCharacteristic* characteristic) const {
  return GetMockCBPeripheral(characteristic->GetService());
}

MockCBCharacteristic* BluetoothTest::GetCBMockCharacteristic(
    BluetoothRemoteGattCharacteristic* characteristic) const {
  device::BluetoothRemoteGattCharacteristicMac* mac_gatt_characteristic =
      static_cast<device::BluetoothRemoteGattCharacteristicMac*>(
          characteristic);
  CBCharacteristic* cb_characteristic =
      mac_gatt_characteristic->GetCBCharacteristic();
  return ObjCCast<MockCBCharacteristic>(cb_characteristic);
}

// Utility function for generating new (CBUUID, address) pairs where CBUUID
// hashes to address. For use when adding a new device address to the testing
// suite because CoreBluetooth peripherals have CBUUIDs in place of addresses,
// and we construct fake addresses for them by hashing the CBUUID. By changing
// |target| the user can generate sequentially numbered test addresses.
//
// std::string BluetoothTestMac::FindCBUUIDForHashTarget() {
//   // The desired first 6 digits of the hash.  For example 0100000, 020000,
//   // 030000, ...
//   const std::string target = "010000";
//   // 128 bit buffer to be encoded as a hex string.
//   int64_t input[2] = {0};
//   // There are 2^24 ~ 10^7 possible configurations for the first 6 digits,
//   // ie. each input has probability 10^-7 of succeeding, under the dubious
//   // assumption that traversing inputs sequentially is as good as traversing
//   // them randomly. After 10^8 iterations then the probability of never
//   // succeeding is ((10^7-1)/10^7)^(10^8) ~= 10^-5.
//   while (input[0] < LLONG_MAX) {
//     // Encode as a hexidecimal number.  Note that on x86 input[0] is stored
//     // as a little-endian number, and so read backwards by HexEncode.
//     std::string input_str = base::HexEncode(&input, sizeof(input));
//     input_str.insert(20, "-");
//     input_str.insert(16, "-");
//     input_str.insert(12, "-");
//     input_str.insert(8, "-");
//     char raw[3];
//     crypto::SHA256HashString(input_str, raw, sizeof(raw));
//     if (base::HexEncode(raw, sizeof(raw)) == target) {
//       return input_str;
//     }
//     ++input[0];
//   }
//   return "";
// }

}  // namespace device
