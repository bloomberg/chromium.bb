// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

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

}  // namespace device
