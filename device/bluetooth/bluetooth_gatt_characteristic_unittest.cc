// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"
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
  SimulateGattCharacteristic(service1, uuid);
  SimulateGattCharacteristic(service1, uuid);
  SimulateGattCharacteristic(service2, uuid);
  SimulateGattCharacteristic(service2, uuid);
  SimulateGattCharacteristic(service3, uuid);
  SimulateGattCharacteristic(service3, uuid);
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

}  // namespace device
