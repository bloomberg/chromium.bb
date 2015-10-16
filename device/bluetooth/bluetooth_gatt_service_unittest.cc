// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

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
  StartDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

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

}  // namespace device
