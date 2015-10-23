// Copyright 2014 The Chromium Authors. All rights reserved.
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

#if defined(OS_ANDROID)
TEST_F(BluetoothTest, GetUUIDAndGetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  SimulateGattConnection(device);

  // Create multiple instances with the same UUID.
  BluetoothUUID uuid("00000000-0000-1000-8000-00805f9b34fb");
  std::vector<std::string> services;
  services.push_back(uuid.canonical_value());
  services.push_back(uuid.canonical_value());
  SimulateGattServicesDiscovered(device, services);

  // Each has the same UUID.
  EXPECT_EQ(uuid, device->GetGattServices()[0]->GetUUID());
  EXPECT_EQ(uuid, device->GetGattServices()[1]->GetUUID());

  // Instance IDs are unique.
  EXPECT_NE(device->GetGattServices()[0]->GetIdentifier(),
            device->GetGattServices()[1]->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothTest, GetCharacteristics_FindNone) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  SimulateGattConnection(device);

  // Simulate a service, with no Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  EXPECT_EQ(0u, service->GetCharacteristics().size());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothTest, GetCharacteristic_FindSeveral) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  SimulateGattConnection(device);

  // Simulate a service, with no Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  std::string characteristic_uuid1 = "00000001-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid2 = "00000002-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid3 = characteristic_uuid2;  // Duplicate UUID.
  SimulateGattCharacteristic(service, characteristic_uuid1);
  SimulateGattCharacteristic(service, characteristic_uuid2);
  SimulateGattCharacteristic(service, characteristic_uuid3);

  EXPECT_EQ(3u, service->GetCharacteristics().size());
  // TODO(scheib): Implement GetIdentifier. crbug.com/545682
  //     std::string characteristic_id1 =
  //         service->GetCharacteristics()[0]->GetIdentifier();
  // ...
  // EXPECT_NE(characteristic_id2, characteristic_id3);
  // For now, just hard-code:
  std::string characteristic_id1 = "00000001-0000-1000-8000-00805f9b34fb0";
  std::string characteristic_id2 = "00000002-0000-1000-8000-00805f9b34fb0";
  std::string characteristic_id3 = "00000002-0000-1000-8000-00805f9b34fb1";
  EXPECT_TRUE(service->GetCharacteristic(characteristic_id1));
  EXPECT_TRUE(service->GetCharacteristic(characteristic_id2));
  EXPECT_TRUE(service->GetCharacteristic(characteristic_id3));
}
#endif  // defined(OS_ANDROID)

}  // namespace device
