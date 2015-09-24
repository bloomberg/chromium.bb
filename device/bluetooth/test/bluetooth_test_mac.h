// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_

#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

class BluetoothAdapterMac;

// Mac implementation of BluetoothTestBase.
class BluetoothTestMac : public BluetoothTestBase {
 public:
  static const std::string kTestPeripheralUUID1;
  static const std::string kTestPeripheralUUID2;

  BluetoothTestMac();
  ~BluetoothTestMac() override;

  // Test overrides:
  void SetUp() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal) override;

 protected:
  // Utility function for finding CBUUIDs with relatively nice SHA256 hashes.
  std::string FindCBUUIDForHashTarget();

  BluetoothAdapterMac* adapter_mac_ = NULL;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestMac BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
