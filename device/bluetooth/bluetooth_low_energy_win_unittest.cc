// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kValidDeviceInstanceId[] =
    "BTHLE\\DEV_BC6A29AB5FB0\\8&31038925&0&BC6A29AB5FB0";

const char kInvalidDeviceInstanceId[] =
    "BTHLE\\BC6A29AB5FB0_DEV_\\8&31038925&0&BC6A29AB5FB0";

}  // namespace

namespace device {

class BluetoothLowEnergyWinTest : public testing::Test {};

TEST_F(BluetoothLowEnergyWinTest, ExtractValidBluetoothAddress) {
  BLUETOOTH_ADDRESS btha;
  std::string error;
  bool success =
      device::win::ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
          kValidDeviceInstanceId, &btha, &error);

  EXPECT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(0xbc, btha.rgBytes[5]);
  EXPECT_EQ(0x6a, btha.rgBytes[4]);
  EXPECT_EQ(0x29, btha.rgBytes[3]);
  EXPECT_EQ(0xab, btha.rgBytes[2]);
  EXPECT_EQ(0x5f, btha.rgBytes[1]);
  EXPECT_EQ(0xb0, btha.rgBytes[0]);
}

TEST_F(BluetoothLowEnergyWinTest, ExtractInvalidBluetoothAddress) {
  BLUETOOTH_ADDRESS btha;
  std::string error;
  bool success =
      device::win::ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
          kInvalidDeviceInstanceId, &btha, &error);

  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());
}

}  // namespace device
