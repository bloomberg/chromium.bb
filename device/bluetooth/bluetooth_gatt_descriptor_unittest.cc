// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
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
class BluetoothGattDescriptorTest : public BluetoothTest {};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothGattDescriptorTest, GetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify that descriptors on them have distinct IDs.
  BluetoothDevice* device1 = DiscoverLowEnergyDevice(3);
  BluetoothDevice* device2 = DiscoverLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
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
  // 6 descriptors (same UUID), 1 on each characteristic
  // TODO(crbug.com/576900) Test multiple descriptors with same UUID on one
  // characteristic.
  SimulateGattDescriptor(char1, uuid);
  SimulateGattDescriptor(char2, uuid);
  SimulateGattDescriptor(char3, uuid);
  SimulateGattDescriptor(char4, uuid);
  SimulateGattDescriptor(char5, uuid);
  SimulateGattDescriptor(char6, uuid);
  BluetoothGattDescriptor* desc1 = char1->GetDescriptors()[0];
  BluetoothGattDescriptor* desc2 = char2->GetDescriptors()[0];
  BluetoothGattDescriptor* desc3 = char3->GetDescriptors()[0];
  BluetoothGattDescriptor* desc4 = char4->GetDescriptors()[0];
  BluetoothGattDescriptor* desc5 = char5->GetDescriptors()[0];
  BluetoothGattDescriptor* desc6 = char6->GetDescriptors()[0];

  // All IDs are unique.
  EXPECT_NE(desc1->GetIdentifier(), desc2->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc2->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc3->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc4->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc4->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc5->GetIdentifier(), desc6->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattDescriptorTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  ASSERT_EQ(1u, device->GetGattServices().size());
  BluetoothGattService* service = device->GetGattServices()[0];

  SimulateGattCharacteristic(service, "00000000-0000-1000-8000-00805f9b34fb",
                             /* properties */ 0);
  ASSERT_EQ(1u, service->GetCharacteristics().size());
  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristics()[0];

  // Create 2 descriptors.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattDescriptor(characteristic, uuid_str1);
  SimulateGattDescriptor(characteristic, uuid_str2);
  ASSERT_EQ(2u, characteristic->GetDescriptors().size());
  BluetoothGattDescriptor* descriptor1 = characteristic->GetDescriptors()[0];
  BluetoothGattDescriptor* descriptor2 = characteristic->GetDescriptors()[1];

  // Swap as needed to have descriptor1 be the one with uuid1.
  if (descriptor2->GetUUID() == uuid1)
    std::swap(descriptor1, descriptor2);

  EXPECT_EQ(uuid1, descriptor1->GetUUID());
  EXPECT_EQ(uuid2, descriptor2->GetUUID());
}
#endif  // defined(OS_ANDROID)

}  // namespace device
