// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/scan_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace bluetooth {

TEST(ScanFilterTest, Matches) {
  static const char kName[] = "foo";
  static const bluetooth_v2_shlib::Uuid kUuid = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};

  ScanFilter name;
  name.name = kName;

  ScanFilter uuid;
  uuid.service_uuid = kUuid;

  ScanFilter name_and_uuid;
  name_and_uuid.name = kName;
  name_and_uuid.service_uuid = kUuid;

  LeScanResult result;

  const ScanFilter empty;
  EXPECT_TRUE(empty.Matches(result));
  EXPECT_FALSE(name.Matches(result));
  EXPECT_FALSE(name_and_uuid.Matches(result));

  result.type_to_data[LeScanResult::kGapShortName].emplace_back(
      reinterpret_cast<const uint8_t*>(kName),
      reinterpret_cast<const uint8_t*>(kName) + strlen(kName));
  EXPECT_TRUE(name.Matches(result));
  EXPECT_FALSE(name_and_uuid.Matches(result));

  ++result.type_to_data[LeScanResult::kGapShortName][0][0];
  EXPECT_FALSE(name.Matches(result));
  EXPECT_FALSE(uuid.Matches(result));
  EXPECT_FALSE(name_and_uuid.Matches(result));

  result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kUuid.rbegin(), kUuid.rend());
  EXPECT_TRUE(uuid.Matches(result));
  EXPECT_FALSE(name_and_uuid.Matches(result));

  --result.type_to_data[LeScanResult::kGapShortName][0][0];
  EXPECT_TRUE(name_and_uuid.Matches(result));

  ++result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids][0][0];
  EXPECT_FALSE(uuid.Matches(result));
  EXPECT_FALSE(name_and_uuid.Matches(result));
}

}  // namespace bluetooth
}  // namespace chromecast
