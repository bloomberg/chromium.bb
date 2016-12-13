// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BluetoothDeviceChooserControllerTest : public testing::Test {};

TEST_F(BluetoothDeviceChooserControllerTest, CalculateSignalStrengthLevel) {
  EXPECT_EQ(
      0, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-128));
  EXPECT_EQ(
      0, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-72));

  EXPECT_EQ(
      1, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-71));
  EXPECT_EQ(
      1, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-64));

  EXPECT_EQ(
      2, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-63));
  EXPECT_EQ(
      2, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-56));

  EXPECT_EQ(
      3, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-55));
  EXPECT_EQ(
      3, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-48));

  EXPECT_EQ(
      4, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-47));
  EXPECT_EQ(
      4, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(127));
}

}  // namespace content