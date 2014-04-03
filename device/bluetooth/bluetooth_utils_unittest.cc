// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace bluetooth_utils {

TEST(BluetoothUtilsTest, CanonicalUuid) {
  // Does nothing for an already canonical UUID
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("00001101-0000-1000-8000-00805f9b34fb"));

  // Rejects misformatted
  EXPECT_EQ("", CanonicalUuid("1101a"));
  EXPECT_EQ("", CanonicalUuid("Z101"));
  EXPECT_EQ("", CanonicalUuid("0000-1101"));
  EXPECT_EQ("", CanonicalUuid("0000Z101"));
  EXPECT_EQ("", CanonicalUuid("0001101-0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("", CanonicalUuid("Z0001101-0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("", CanonicalUuid("00001101 0000-1000-8000-00805f9b34fb"));
  EXPECT_EQ("", CanonicalUuid("00001101-0000:1000-8000-00805f9b34fb"));
  EXPECT_EQ("", CanonicalUuid("00001101-0000-1000;8000-00805f9b34fb"));
  EXPECT_EQ("", CanonicalUuid("00001101-0000-1000-8000000805f9b34fb"));

  // Lower case
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("00001101-0000-1000-8000-00805F9B34FB"));

  // Short to full
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("1101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("0x1101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("00001101"));
  EXPECT_EQ("00001101-0000-1000-8000-00805f9b34fb",
      CanonicalUuid("0x00001101"));

  // No 0x prefix on 36 character
  EXPECT_EQ("", CanonicalUuid("0x00001101-0000-1000-8000-00805f9b34fb"));
}

TEST(BluetoothUtilsTest, UUID) {
  const char kValid128Bit0[] = "12345678-1234-5678-9abc-def123456789";
  const char kValid128Bit1[] = "00001101-0000-1000-8000-00805f9b34fb";
  const char kInvalid36Char[] = "1234567-1234-5678-9abc-def123456789";
  const char kInvalid4Char[] = "Z101";
  const char kValid16Bit[] = "0x1101";
  const char kValid32Bit[] = "00001101";

  // Valid 128-bit custom UUID.
  UUID uuid0(kValid128Bit0);
  EXPECT_TRUE(uuid0.IsValid());
  EXPECT_EQ(UUID::kFormat128Bit, uuid0.format());
  EXPECT_EQ(uuid0.value(), uuid0.canonical_value());

  // Valid 128-bit UUID.
  UUID uuid1(kValid128Bit1);
  EXPECT_TRUE(uuid1.IsValid());
  EXPECT_EQ(UUID::kFormat128Bit, uuid1.format());
  EXPECT_EQ(uuid1.value(), uuid1.canonical_value());

  EXPECT_NE(uuid0, uuid1);

  // Invalid 128-bit UUID.
  UUID uuid2(kInvalid36Char);
  EXPECT_FALSE(uuid2.IsValid());
  EXPECT_EQ(UUID::kFormatInvalid, uuid2.format());
  EXPECT_TRUE(uuid2.value().empty());
  EXPECT_TRUE(uuid2.canonical_value().empty());

  // Invalid 16-bit UUID.
  UUID uuid3(kInvalid4Char);
  EXPECT_FALSE(uuid3.IsValid());
  EXPECT_EQ(UUID::kFormatInvalid, uuid3.format());
  EXPECT_TRUE(uuid3.value().empty());
  EXPECT_TRUE(uuid3.canonical_value().empty());

  // Valid 16-bit UUID.
  UUID uuid4(kValid16Bit);
  EXPECT_TRUE(uuid4.IsValid());
  EXPECT_EQ(UUID::kFormat16Bit, uuid4.format());
  EXPECT_NE(uuid4.value(), uuid4.canonical_value());
  EXPECT_EQ("1101", uuid4.value());
  EXPECT_EQ(kValid128Bit1, uuid4.canonical_value());

  // Valid 32-bit UUID.
  UUID uuid5(kValid32Bit);
  EXPECT_TRUE(uuid5.IsValid());
  EXPECT_EQ(UUID::kFormat32Bit, uuid5.format());
  EXPECT_NE(uuid5.value(), uuid5.canonical_value());
  EXPECT_EQ("00001101", uuid5.value());
  EXPECT_EQ(kValid128Bit1, uuid5.canonical_value());

  // uuid4, uuid5, and uuid1 are equivalent.
  EXPECT_EQ(uuid4, uuid5);
  EXPECT_EQ(uuid1, uuid4);
  EXPECT_EQ(uuid1, uuid5);
}

}  // namespace bluetooth_utils
}  // namespace device
