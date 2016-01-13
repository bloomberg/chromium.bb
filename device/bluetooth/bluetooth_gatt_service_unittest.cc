// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

#include "build/build_config.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
class BluetoothGattServiceTest : public BluetoothTest {};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothGattServiceTest, GetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = DiscoverLowEnergyDevice(3);
  BluetoothDevice* device2 = DiscoverLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);

  // 2 duplicate UUIDs creating 2 service instances on each device.
  std::vector<std::string> services;
  std::string uuid = "00000000-0000-1000-8000-00805f9b34fb";
  services.push_back(uuid);
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device1, services);
  SimulateGattServicesDiscovered(device2, services);
  BluetoothGattService* service1 = device1->GetGattServices()[0];
  BluetoothGattService* service2 = device1->GetGattServices()[1];
  BluetoothGattService* service3 = device2->GetGattServices()[0];
  BluetoothGattService* service4 = device2->GetGattServices()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(service1->GetIdentifier(), service2->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service2->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service2->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service3->GetIdentifier(), service4->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattServiceTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
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
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattServiceTest, GetCharacteristics_FindNone) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
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
TEST_F(BluetoothGattServiceTest, GetCharacteristics_and_GetCharacteristic) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Simulate a service, with several Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid1 = "11111111-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid2 = "22222222-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid3 = characteristic_uuid2;  // Duplicate UUID.
  std::string characteristic_uuid4 = "33333333-0000-1000-8000-00805f9b34fb";
  SimulateGattCharacteristic(service, characteristic_uuid1, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid2, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid3, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid4, /* properties */ 0);

  // Verify that GetCharacteristic can retrieve characteristics again by ID,
  // and that the same Characteristics come back.
  EXPECT_EQ(4u, service->GetCharacteristics().size());
  std::string char_id1 = service->GetCharacteristics()[0]->GetIdentifier();
  std::string char_id2 = service->GetCharacteristics()[1]->GetIdentifier();
  std::string char_id3 = service->GetCharacteristics()[2]->GetIdentifier();
  std::string char_id4 = service->GetCharacteristics()[3]->GetIdentifier();
  BluetoothUUID char_uuid1 = service->GetCharacteristics()[0]->GetUUID();
  BluetoothUUID char_uuid2 = service->GetCharacteristics()[1]->GetUUID();
  BluetoothUUID char_uuid3 = service->GetCharacteristics()[2]->GetUUID();
  BluetoothUUID char_uuid4 = service->GetCharacteristics()[3]->GetUUID();
  EXPECT_EQ(char_uuid1, service->GetCharacteristic(char_id1)->GetUUID());
  EXPECT_EQ(char_uuid2, service->GetCharacteristic(char_id2)->GetUUID());
  EXPECT_EQ(char_uuid3, service->GetCharacteristic(char_id3)->GetUUID());
  EXPECT_EQ(char_uuid4, service->GetCharacteristic(char_id4)->GetUUID());

  // GetCharacteristics & GetCharacteristic return the same object for the same
  // ID:
  EXPECT_EQ(service->GetCharacteristics()[0],
            service->GetCharacteristic(char_id1));
  EXPECT_EQ(service->GetCharacteristic(char_id1),
            service->GetCharacteristic(char_id1));
}
#endif  // defined(OS_ANDROID)

}  // namespace device
