// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_mac.h"

#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else  // !defined(OS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif  // defined(OS_IOS)

namespace device {

namespace {

CBPeripheral* CreateMockPeripheral(NSString* peripheral_identifier) {
  Class peripheral_class = NSClassFromString(@"CBPeripheral");
  id mock_peripheral = [OCMockObject mockForClass:[peripheral_class class]];
  [[[mock_peripheral stub] andReturnValue:@(CBPeripheralStateDisconnected)]
      performSelector:@selector(state)];
  Class uuid_class = NSClassFromString(@"NSUUID");
  [[[mock_peripheral stub]
      andReturn:[[uuid_class performSelector:@selector(UUID)]
                    performSelector:@selector(initWithUUIDString:)
                         withObject:peripheral_identifier]]
      performSelector:@selector(identifier)];
  [[[mock_peripheral stub]
      andReturn:[NSString stringWithUTF8String:BluetoothTest::kTestDeviceName
                                                   .c_str()]] name];
  return mock_peripheral;
}

NSDictionary* CreateAdvertisementData(NSString* name, NSArray* uuids) {
  NSMutableDictionary* advertisement_data =
      [NSMutableDictionary dictionaryWithDictionary:@{
        CBAdvertisementDataLocalNameKey : name,
        CBAdvertisementDataServiceDataKey : [NSDictionary dictionary],
        CBAdvertisementDataIsConnectable : @(YES),
      }];
  if (uuids)
    [advertisement_data setObject:uuids
                           forKey:CBAdvertisementDataServiceUUIDsKey];
  return advertisement_data;
}

}  // namespace

// UUID1 hashes to kTestDeviceAddress1, and UUID2 to kTestDeviceAddress2.
const std::string BluetoothTestMac::kTestPeripheralUUID1 =
    "34045B00-0000-0000-0000-000000000000";
const std::string BluetoothTestMac::kTestPeripheralUUID2 =
    "EC1B8F00-0000-0000-0000-000000000000";

BluetoothTestMac::BluetoothTestMac() {}

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
    id low_energy_central_manager = [[MockCentralManager alloc] init];
    [low_energy_central_manager setState:CBCentralManagerStateUnsupported];
    adapter_mac_->SetCentralManagerForTesting(low_energy_central_manager);
  }
}

void BluetoothTestMac::InitWithFakeAdapter() {
  adapter_mac_ =
      BluetoothAdapterMac::CreateAdapterForTest(
          kTestAdapterName, kTestAdapterAddress, message_loop_.task_runner())
          .get();
  adapter_ = adapter_mac_;

  if (BluetoothAdapterMac::IsLowEnergyAvailable()) {
    id low_energy_central_manager = [[MockCentralManager alloc] init];
    [low_energy_central_manager setState:CBCentralManagerStatePoweredOn];
    adapter_mac_->SetCentralManagerForTesting(low_energy_central_manager);
  }
}

BluetoothDevice* BluetoothTestMac::DiscoverLowEnergyDevice(int device_ordinal) {
  TestBluetoothAdapterObserver observer(adapter_);
  CBCentralManager* central_manager = adapter_mac_->low_energy_central_manager_;
  BluetoothLowEnergyCentralManagerDelegate* central_manager_delegate =
      adapter_mac_->low_energy_central_manager_delegate_;
  Class cbuuid_class = NSClassFromString(@"CBUUID");
  switch (device_ordinal) {
    case 1: {
      CBPeripheral* peripheral = CreateMockPeripheral(
          [NSString stringWithUTF8String:kTestPeripheralUUID1.c_str()]);
      NSString* name = [NSString stringWithUTF8String:kTestDeviceName.c_str()];
      NSArray* uuids = @[
        [cbuuid_class
            UUIDWithString:[NSString stringWithUTF8String:kTestUUIDGenericAccess
                                                              .c_str()]],
        [cbuuid_class
            UUIDWithString:[NSString
                               stringWithUTF8String:kTestUUIDGenericAttribute
                                                        .c_str()]]
      ];
      NSDictionary* advertisement_data = CreateAdvertisementData(name, uuids);
      [central_manager_delegate centralManager:central_manager
                         didDiscoverPeripheral:peripheral
                             advertisementData:advertisement_data
                                          RSSI:[NSNumber numberWithInt:0]];
      break;
    }
    case 2: {
      CBPeripheral* peripheral = CreateMockPeripheral(
          [NSString stringWithUTF8String:kTestPeripheralUUID1.c_str()]);
      NSString* name = [NSString stringWithUTF8String:kTestDeviceName.c_str()];
      NSArray* uuids = @[
        [cbuuid_class
            UUIDWithString:[NSString
                               stringWithUTF8String:kTestUUIDImmediateAlert
                                                        .c_str()]],
        [cbuuid_class
            UUIDWithString:[NSString
                               stringWithUTF8String:kTestUUIDLinkLoss.c_str()]]
      ];
      NSDictionary* advertisement_data = CreateAdvertisementData(name, uuids);
      [central_manager_delegate centralManager:central_manager
                         didDiscoverPeripheral:peripheral
                             advertisementData:advertisement_data
                                          RSSI:[NSNumber numberWithInt:0]];
      break;
    }
    case 3: {
      CBPeripheral* peripheral = CreateMockPeripheral(
          [NSString stringWithUTF8String:kTestPeripheralUUID1.c_str()]);
      NSString* name =
          [NSString stringWithUTF8String:kTestDeviceNameEmpty.c_str()];
      NSArray* uuids = nil;
      NSDictionary* advertisement_data = CreateAdvertisementData(name, uuids);
      [central_manager_delegate centralManager:central_manager
                         didDiscoverPeripheral:peripheral
                             advertisementData:advertisement_data
                                          RSSI:[NSNumber numberWithInt:0]];
      break;
    }
    case 4: {
      CBPeripheral* peripheral = CreateMockPeripheral(
          [NSString stringWithUTF8String:kTestPeripheralUUID2.c_str()]);
      NSString* name =
          [NSString stringWithUTF8String:kTestDeviceNameEmpty.c_str()];
      NSArray* uuids = nil;
      NSDictionary* advertisement_data = CreateAdvertisementData(name, uuids);
      [central_manager_delegate centralManager:central_manager
                         didDiscoverPeripheral:peripheral
                             advertisementData:advertisement_data
                                          RSSI:[NSNumber numberWithInt:0]];
      break;
    }
  }
  return observer.last_device();
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
