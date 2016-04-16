// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_

#include "device/bluetooth/test/bluetooth_test.h"

namespace bluez {
class FakeBluetoothDeviceClient;
}

namespace device {

// BlueZ implementation of BluetoothTestBase.
class BluetoothTestBlueZ : public BluetoothTestBase {
 public:
  BluetoothTestBlueZ();
  ~BluetoothTestBlueZ() override;

  // Test overrides:
  void SetUp() override;
  void TearDown() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal) override;

 private:
  bluez::FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestBlueZ;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_
