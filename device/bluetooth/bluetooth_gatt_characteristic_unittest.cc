// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_characteristic.h"

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
class BluetoothGattCharacteristicTest : public BluetoothTest {};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = DiscoverLowEnergyDevice(3);
  BluetoothDevice* device2 = DiscoverLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(),
                                GetConnectErrorCallback());
  device2->CreateGattConnection(GetGattConnectionCallback(),
                                GetConnectErrorCallback());
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);

  // 3 services (all with same UUID).
  //   1 on the first device (to test characteristic instances across devices).
  //   2 on the second device (to test same device, multiple service instances).
  std::vector<std::string> services;
  std::string uuid = "00000000-0000-1000-8000-00805f9b34fb";
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device1, services);
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device2, services);
  BluetoothGattService* service1 = device1->GetGattServices()[0];
  BluetoothGattService* service2 = device2->GetGattServices()[0];
  BluetoothGattService* service3 = device2->GetGattServices()[1];
  // 6 characteristics (same UUID), 2 on each service.
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  BluetoothGattCharacteristic* char1 = service1->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char2 = service1->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char3 = service2->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char4 = service2->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char5 = service3->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char6 = service3->GetCharacteristics()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(char1->GetIdentifier(), char2->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char2->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char3->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char4->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char4->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char5->GetIdentifier(), char6->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  SimulateGattConnection(device);
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  // Create 3 characteristics. Two of them are duplicates.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattCharacteristic(service, uuid_str1, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  BluetoothGattCharacteristic* char1 = service->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char2 = service->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char3 = service->GetCharacteristics()[2];

  // Swap as needed to have char1 point to the the characteristic with uuid1.
  if (char2->GetUUID() == uuid1) {
    std::swap(char1, char2);
  } else if (char3->GetUUID() == uuid1) {
    std::swap(char1, char3);
  }

  EXPECT_EQ(uuid1, char1->GetUUID());
  EXPECT_EQ(uuid2, char2->GetUUID());
  EXPECT_EQ(uuid2, char3->GetUUID());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetProperties) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  SimulateGattConnection(device);
  std::vector<std::string> services;
  std::string uuid("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  // Create two characteristics with different properties:
  SimulateGattCharacteristic(service, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid, /* properties */ 7);

  // Read the properties. Because ordering is unknown swap as necessary.
  int properties1 = service->GetCharacteristics()[0]->GetProperties();
  int properties2 = service->GetCharacteristics()[1]->GetProperties();
  if (properties2 == 0)
    std::swap(properties1, properties2);
  EXPECT_EQ(0, properties1);
  EXPECT_EQ(7, properties2);
}
#endif  // defined(OS_ANDROID)

}  // namespace device
