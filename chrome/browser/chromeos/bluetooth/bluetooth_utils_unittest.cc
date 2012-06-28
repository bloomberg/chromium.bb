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
  EXPECT_EQ(1, bluetooth_address.b[5]);
  EXPECT_EQ(2, bluetooth_address.b[4]);
  EXPECT_EQ(3, bluetooth_address.b[3]);
  EXPECT_EQ(10, bluetooth_address.b[2]);
  EXPECT_EQ(16, bluetooth_address.b[1]);
  EXPECT_EQ(160, bluetooth_address.b[0]);

  EXPECT_FALSE(bluetooth_utils::str2ba("obviously wrong", &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("00:00", &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("00:00:00:00:00:00:00",
        &bluetooth_address));
  EXPECT_FALSE(bluetooth_utils::str2ba("01:02:03:0A:10:A0", NULL));
}

TEST(BluetoothUtilsTest, CanonicalUuid) {
  // Does nothing for an already canonical UUID
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("00001101-0000-1000-8000-00805f9b34fb"));

  // Rejects misformatted
  EXPECT_EQ("", bluetooth_utils::CanonicalUuid("1101a"));
  EXPECT_EQ("", bluetooth_utils::CanonicalUuid("Z101"));
  EXPECT_EQ("", bluetooth_utils::CanonicalUuid("0000-1101"));
  EXPECT_EQ("", bluetooth_utils::CanonicalUuid("0000Z101"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("0001101-0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("Z0001101-0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("00001101 0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("00001101-0000:1000-8000-00805f9b34fb"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("00001101-0000-1000;8000-00805f9b34fb"));
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("00001101-0000-1000-8000000805f9b34fb"));

  // Lower case
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("00001101-0000-1000-8000-00805F9B34FB"));

  // Short to full
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("1101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("0x1101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("00001101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      bluetooth_utils::CanonicalUuid("0x00001101"));

  // No 0x prefix on 36 character
  EXPECT_EQ("",
      bluetooth_utils::CanonicalUuid("0x00001101-0000-1000-8000-00805f9b34fb"));
}

}  // namespace chromeos
