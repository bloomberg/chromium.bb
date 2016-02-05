// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blacklist.h"

#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothUUID;

class BluetoothBlacklistTest : public ::testing::Test {
 public:
  BluetoothBlacklistTest() {
    // Because BluetoothBlacklist is used via a singleton instance, the data
    // must be reset for each test.
    content::BluetoothBlacklist::Get().ResetToDefaultValuesForTest();
  }
};

namespace content {

TEST_F(BluetoothBlacklistTest, NonExcludedUUID) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  BluetoothUUID non_excluded_uuid("00000000-0000-0000-0000-000000000000");
  EXPECT_FALSE(blacklist.IsExcluded(non_excluded_uuid));
  EXPECT_FALSE(blacklist.IsExcludedFromReads(non_excluded_uuid));
  EXPECT_FALSE(blacklist.IsExcludedFromWrites(non_excluded_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeUUID) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  BluetoothUUID excluded_uuid("eeee");
  blacklist.AddOrDie(excluded_uuid, BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(blacklist.IsExcluded(excluded_uuid));
  EXPECT_TRUE(blacklist.IsExcludedFromReads(excluded_uuid));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(excluded_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeReadsUUID) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  BluetoothUUID exclude_reads_uuid("eeee");
  blacklist.AddOrDie(exclude_reads_uuid,
                     BluetoothBlacklist::Value::EXCLUDE_READS);
  EXPECT_FALSE(blacklist.IsExcluded(exclude_reads_uuid));
  EXPECT_TRUE(blacklist.IsExcludedFromReads(exclude_reads_uuid));
  EXPECT_FALSE(blacklist.IsExcludedFromWrites(exclude_reads_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeWritesUUID) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  BluetoothUUID exclude_writes_uuid("eeee");
  blacklist.AddOrDie(exclude_writes_uuid,
                     BluetoothBlacklist::Value::EXCLUDE_WRITES);
  EXPECT_FALSE(blacklist.IsExcluded(exclude_writes_uuid));
  EXPECT_FALSE(blacklist.IsExcludedFromReads(exclude_writes_uuid));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(exclude_writes_uuid));
}

// Abreviated UUIDs used to create, or test against, the blacklist work
// correctly compared to full UUIDs.
TEST_F(BluetoothBlacklistTest, AbreviatedUUIDs) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();

  blacklist.AddOrDie(BluetoothUUID("aaaa"), BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(blacklist.IsExcluded(
      BluetoothUUID("0000aaaa-0000-1000-8000-00805f9b34fb")));

  blacklist.AddOrDie(BluetoothUUID("0000bbbb-0000-1000-8000-00805f9b34fb"),
                     BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(blacklist.IsExcluded(BluetoothUUID("bbbb")));
}

TEST_F(BluetoothBlacklistTest, IsExcluded_BluetoothScanFilter_ReturnsFalse) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  blacklist.AddOrDie(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  blacklist.AddOrDie(BluetoothUUID("ee01"),
                     BluetoothBlacklist::Value::EXCLUDE_READS);
  blacklist.AddOrDie(BluetoothUUID("ee02"),
                     BluetoothBlacklist::Value::EXCLUDE_WRITES);
  {
    std::vector<BluetoothScanFilter> empty_filters;
    EXPECT_FALSE(blacklist.IsExcluded(empty_filters));
  }
  {
    std::vector<BluetoothScanFilter> single_empty_filter(1);
    EXPECT_EQ(0u, single_empty_filter[0].services.size());
    EXPECT_FALSE(blacklist.IsExcluded(single_empty_filter));
  }
  {
    std::vector<BluetoothScanFilter> single_non_matching_filter(1);
    single_non_matching_filter[0].services.push_back(BluetoothUUID("0000"));
    EXPECT_FALSE(blacklist.IsExcluded(single_non_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> multiple_non_matching_filter(2);
    multiple_non_matching_filter[0].services.push_back(BluetoothUUID("0000"));
    multiple_non_matching_filter[0].services.push_back(BluetoothUUID("ee01"));
    multiple_non_matching_filter[1].services.push_back(BluetoothUUID("ee02"));
    multiple_non_matching_filter[1].services.push_back(BluetoothUUID("0003"));
    EXPECT_FALSE(blacklist.IsExcluded(multiple_non_matching_filter));
  }
}

TEST_F(BluetoothBlacklistTest, IsExcluded_BluetoothScanFilter_ReturnsTrue) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  blacklist.AddOrDie(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  {
    std::vector<BluetoothScanFilter> single_matching_filter(1);
    single_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(blacklist.IsExcluded(single_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> first_matching_filter(2);
    first_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    first_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    first_matching_filter[1].services.push_back(BluetoothUUID("0002"));
    first_matching_filter[1].services.push_back(BluetoothUUID("0003"));
    EXPECT_TRUE(blacklist.IsExcluded(first_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> last_matching_filter(2);
    last_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    last_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    last_matching_filter[1].services.push_back(BluetoothUUID("0002"));
    last_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(blacklist.IsExcluded(last_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> multiple_matching_filter(2);
    multiple_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(blacklist.IsExcluded(multiple_matching_filter));
  }
}

TEST_F(BluetoothBlacklistTest, RemoveExcludedUuids_NonMatching) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  blacklist.AddOrDie(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  blacklist.AddOrDie(BluetoothUUID("ee01"),
                     BluetoothBlacklist::Value::EXCLUDE_READS);
  blacklist.AddOrDie(BluetoothUUID("ee02"),
                     BluetoothBlacklist::Value::EXCLUDE_WRITES);
  {
    std::vector<BluetoothUUID> empty;
    std::vector<BluetoothUUID> expected_empty;
    blacklist.RemoveExcludedUuids(&empty);
    EXPECT_EQ(expected_empty, empty);
  }
  {
    std::vector<BluetoothUUID> single_non_matching;
    single_non_matching.push_back(BluetoothUUID("0000"));

    std::vector<BluetoothUUID> expected_copy(single_non_matching);

    blacklist.RemoveExcludedUuids(&single_non_matching);
    EXPECT_EQ(expected_copy, single_non_matching);
  }
  {
    std::vector<BluetoothUUID> multiple_non_matching;
    multiple_non_matching.push_back(BluetoothUUID("0000"));
    multiple_non_matching.push_back(BluetoothUUID("ee01"));
    multiple_non_matching.push_back(BluetoothUUID("ee02"));
    multiple_non_matching.push_back(BluetoothUUID("0003"));

    std::vector<BluetoothUUID> expected_copy(multiple_non_matching);

    blacklist.RemoveExcludedUuids(&multiple_non_matching);
    EXPECT_EQ(expected_copy, multiple_non_matching);
  }
}

TEST_F(BluetoothBlacklistTest, RemoveExcludedUuids_Matching) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  blacklist.AddOrDie(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  blacklist.AddOrDie(BluetoothUUID("eee2"), BluetoothBlacklist::Value::EXCLUDE);
  blacklist.AddOrDie(BluetoothUUID("eee3"), BluetoothBlacklist::Value::EXCLUDE);
  blacklist.AddOrDie(BluetoothUUID("eee4"), BluetoothBlacklist::Value::EXCLUDE);
  {
    std::vector<BluetoothUUID> single_matching;
    single_matching.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected_empty;

    blacklist.RemoveExcludedUuids(&single_matching);
    EXPECT_EQ(expected_empty, single_matching);
  }
  {
    std::vector<BluetoothUUID> single_matching_of_many;
    single_matching_of_many.push_back(BluetoothUUID("0000"));
    single_matching_of_many.push_back(BluetoothUUID("eeee"));
    single_matching_of_many.push_back(BluetoothUUID("0001"));

    std::vector<BluetoothUUID> expected;
    expected.push_back(BluetoothUUID("0000"));
    expected.push_back(BluetoothUUID("0001"));

    blacklist.RemoveExcludedUuids(&single_matching_of_many);
    EXPECT_EQ(expected, single_matching_of_many);
  }
  {
    std::vector<BluetoothUUID> all_matching_of_many;
    all_matching_of_many.push_back(BluetoothUUID("eee2"));
    all_matching_of_many.push_back(BluetoothUUID("eee4"));
    all_matching_of_many.push_back(BluetoothUUID("eee3"));
    all_matching_of_many.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected_empty;

    blacklist.RemoveExcludedUuids(&all_matching_of_many);
    EXPECT_EQ(expected_empty, all_matching_of_many);
  }
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultBlacklistSize) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  // When adding items to the blacklist the new values should be added in the
  // tests below for each exclusion type.
  EXPECT_EQ(6u, blacklist.size());
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeList) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  EXPECT_TRUE(blacklist.IsExcluded(BluetoothUUID("1800")));
  EXPECT_TRUE(blacklist.IsExcluded(BluetoothUUID("1801")));
  EXPECT_TRUE(blacklist.IsExcluded(BluetoothUUID("1812")));
  EXPECT_TRUE(blacklist.IsExcluded(BluetoothUUID("2a25")));
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeReadList) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  EXPECT_TRUE(blacklist.IsExcludedFromReads(BluetoothUUID("1800")));
  EXPECT_TRUE(blacklist.IsExcludedFromReads(BluetoothUUID("1801")));
  EXPECT_TRUE(blacklist.IsExcludedFromReads(BluetoothUUID("1812")));
  EXPECT_TRUE(blacklist.IsExcludedFromReads(BluetoothUUID("2a25")));
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeWriteList) {
  BluetoothBlacklist& blacklist = BluetoothBlacklist::Get();
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("1800")));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("1801")));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("1812")));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("2a25")));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("2902")));
  EXPECT_TRUE(blacklist.IsExcludedFromWrites(BluetoothUUID("2903")));
}

}  // namespace content
