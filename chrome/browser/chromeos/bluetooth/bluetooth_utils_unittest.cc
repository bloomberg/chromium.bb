// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bluetooth/bluetooth.h>

#include "chrome/browser/chromeos/bluetooth/bluetooth_utils.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace chromeos {

TEST(BluetoothUtilsTest, str2ba) {
  bdaddr_t bluetooth_address;

  EXPECT_TRUE(bluetooth_utils::str2ba("01:02:03:0A:10:A0", &bluetooth_address));
  EXPECT_EQ(1, bluetooth_address.b[0]);
  EXPECT_EQ(2, bluetooth_address.b[1]);
  EXPECT_EQ(3, bluetooth_address.b[2]);
  EXPECT_EQ(10, bluetooth_address.b[3]);
  EXPECT_EQ(16, bluetooth_address.b[4]);
  EXPECT_EQ(160, bluetooth_address.b[5]);

  EXPECT_FALSE(bluetooth_utils::str2ba("obviously wrong", &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("00:00", &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("00:00:00:00:00:00:00",
        &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("01:02:03:0A:10:A0", NULL));
}

}  // namespace chromeos
