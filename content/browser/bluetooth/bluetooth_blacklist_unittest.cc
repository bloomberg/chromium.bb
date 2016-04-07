// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blacklist.h"

#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothUUID;

namespace content {

class BluetoothBlacklistTest : public ::testing::Test {
 public:
  BluetoothBlacklistTest() : list_(BluetoothBlacklist::Get()) {
    // Because BluetoothBlacklist is used via a singleton instance, the data
    // must be reset for each test.
    list_.ResetToDefaultValuesForTest();
  }
  BluetoothBlacklist& list_;
};

TEST_F(BluetoothBlacklistTest, NonExcludedUUID) {
  BluetoothUUID non_excluded_uuid("00000000-0000-0000-0000-000000000000");
  EXPECT_FALSE(list_.IsExcluded(non_excluded_uuid));
  EXPECT_FALSE(list_.IsExcludedFromReads(non_excluded_uuid));
  EXPECT_FALSE(list_.IsExcludedFromWrites(non_excluded_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeUUID) {
  BluetoothUUID excluded_uuid("eeee");
  list_.Add(excluded_uuid, BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(list_.IsExcluded(excluded_uuid));
  EXPECT_TRUE(list_.IsExcludedFromReads(excluded_uuid));
  EXPECT_TRUE(list_.IsExcludedFromWrites(excluded_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeReadsUUID) {
  BluetoothUUID exclude_reads_uuid("eeee");
  list_.Add(exclude_reads_uuid, BluetoothBlacklist::Value::EXCLUDE_READS);
  EXPECT_FALSE(list_.IsExcluded(exclude_reads_uuid));
  EXPECT_TRUE(list_.IsExcludedFromReads(exclude_reads_uuid));
  EXPECT_FALSE(list_.IsExcludedFromWrites(exclude_reads_uuid));
}

TEST_F(BluetoothBlacklistTest, ExcludeWritesUUID) {
  BluetoothUUID exclude_writes_uuid("eeee");
  list_.Add(exclude_writes_uuid, BluetoothBlacklist::Value::EXCLUDE_WRITES);
  EXPECT_FALSE(list_.IsExcluded(exclude_writes_uuid));
  EXPECT_FALSE(list_.IsExcludedFromReads(exclude_writes_uuid));
  EXPECT_TRUE(list_.IsExcludedFromWrites(exclude_writes_uuid));
}

TEST_F(BluetoothBlacklistTest, InvalidUUID) {
  BluetoothUUID empty_string_uuid("");
  EXPECT_DEATH_IF_SUPPORTED(
      list_.Add(empty_string_uuid, BluetoothBlacklist::Value::EXCLUDE), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcluded(empty_string_uuid), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcludedFromReads(empty_string_uuid), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcludedFromWrites(empty_string_uuid), "");

  BluetoothUUID invalid_string_uuid("Not a valid UUID string.");
  EXPECT_DEATH_IF_SUPPORTED(
      list_.Add(invalid_string_uuid, BluetoothBlacklist::Value::EXCLUDE), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcluded(invalid_string_uuid), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcludedFromReads(invalid_string_uuid), "");
  EXPECT_DEATH_IF_SUPPORTED(list_.IsExcludedFromWrites(invalid_string_uuid),
                            "");
}

// Abreviated UUIDs used to create, or test against, the blacklist work
// correctly compared to full UUIDs.
TEST_F(BluetoothBlacklistTest, AbreviatedUUIDs) {
  list_.Add(BluetoothUUID("aaaa"), BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(
      list_.IsExcluded(BluetoothUUID("0000aaaa-0000-1000-8000-00805f9b34fb")));

  list_.Add(BluetoothUUID("0000bbbb-0000-1000-8000-00805f9b34fb"),
            BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("bbbb")));
}

// Tests permutations of previous values and then Add() with a new value,
// requiring result to be strictest result of the combination.
TEST_F(BluetoothBlacklistTest, Add_MergingExcludeValues) {
  list_.Add(BluetoothUUID("ee01"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("ee01"), BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee01")));

  list_.Add(BluetoothUUID("ee02"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("ee02"), BluetoothBlacklist::Value::EXCLUDE_READS);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee02")));

  list_.Add(BluetoothUUID("ee03"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("ee03"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee03")));

  list_.Add(BluetoothUUID("ee04"), BluetoothBlacklist::Value::EXCLUDE_READS);
  list_.Add(BluetoothUUID("ee04"), BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee04")));

  list_.Add(BluetoothUUID("ee05"), BluetoothBlacklist::Value::EXCLUDE_READS);
  list_.Add(BluetoothUUID("ee05"), BluetoothBlacklist::Value::EXCLUDE_READS);
  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("ee05")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("ee05")));

  list_.Add(BluetoothUUID("ee06"), BluetoothBlacklist::Value::EXCLUDE_READS);
  list_.Add(BluetoothUUID("ee06"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee06")));

  list_.Add(BluetoothUUID("ee07"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  list_.Add(BluetoothUUID("ee07"), BluetoothBlacklist::Value::EXCLUDE);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee07")));

  list_.Add(BluetoothUUID("ee08"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  list_.Add(BluetoothUUID("ee08"), BluetoothBlacklist::Value::EXCLUDE_READS);
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("ee08")));

  list_.Add(BluetoothUUID("ee09"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  list_.Add(BluetoothUUID("ee09"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("ee09")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("ee09")));
}

// Tests Add() with string that contains many UUID:exclusion value pairs,
// checking that the correct blacklist entries are created for them.
TEST_F(BluetoothBlacklistTest, Add_StringWithValidEntries) {
  list_.Add(
      "0001:e,0002:r,0003:w, "  // Single items.
      "0004:r,0004:r, "         // Duplicate items.
      "0005:r,0005:w, "         // Items that merge.
      "00000006:e, "            // 8 char UUID.
      "00000007-0000-1000-8000-00805f9b34fb:e");

  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0001")));

  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("0002")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("0002")));

  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("0003")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("0003")));

  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("0004")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("0004")));

  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0005")));
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0006")));
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0007")));
}

// Tests Add() with strings that contain no valid UUID:exclusion value.
TEST_F(BluetoothBlacklistTest, Add_StringsWithNoValidEntries) {
  size_t previous_list_size = list_.size();
  list_.Add("");
  list_.Add("~!@#$%^&*()-_=+[]{}/*-");
  list_.Add(":");
  list_.Add(",");
  list_.Add(",,");
  list_.Add(",:,");
  list_.Add("1234:");
  list_.Add("1234:q");
  list_.Add("1234:E");
  list_.Add("1234:R");
  list_.Add("1234:W");
  list_.Add("1234:ee");
  list_.Add("1234 :e");
  list_.Add("1234: e");
  list_.Add("1:e");
  list_.Add("1:r");
  list_.Add("1:w");
  list_.Add("00001800-0000-1000-8000-00805f9b34fb:ee");
  list_.Add("z0001800-0000-1000-8000-00805f9b34fb:e");
  list_.Add("☯");
  EXPECT_EQ(previous_list_size, list_.size());
}

// Tests Add() with strings that contain exactly one valid UUID:exclusion value
// pair, and optionally other issues in the string that are ignored.
TEST_F(BluetoothBlacklistTest, Add_StringsWithOneValidEntry) {
  size_t previous_list_size = list_.size();
  list_.Add("0001:e");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0001")));

  list_.Add("00000002:e");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0002")));

  list_.Add("00000003-0000-1000-8000-00805f9b34fb:e");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0003")));

  list_.Add(" 0004:e ");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0004")));

  list_.Add(", 0005:e ,");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0005")));

  list_.Add(":, 0006:e ,,no");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0006")));

  list_.Add("0007:, 0008:e");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0008")));

  list_.Add("\r\n0009:e\n\r");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("0009")));
}

TEST_F(BluetoothBlacklistTest, IsExcluded_BluetoothScanFilter_ReturnsFalse) {
  list_.Add(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("ee01"), BluetoothBlacklist::Value::EXCLUDE_READS);
  list_.Add(BluetoothUUID("ee02"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  {
    std::vector<BluetoothScanFilter> empty_filters;
    EXPECT_FALSE(list_.IsExcluded(empty_filters));
  }
  {
    std::vector<BluetoothScanFilter> single_empty_filter(1);
    EXPECT_EQ(0u, single_empty_filter[0].services.size());
    EXPECT_FALSE(list_.IsExcluded(single_empty_filter));
  }
  {
    std::vector<BluetoothScanFilter> single_non_matching_filter(1);
    single_non_matching_filter[0].services.push_back(BluetoothUUID("0000"));
    EXPECT_FALSE(list_.IsExcluded(single_non_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> multiple_non_matching_filter(2);
    multiple_non_matching_filter[0].services.push_back(BluetoothUUID("0000"));
    multiple_non_matching_filter[0].services.push_back(BluetoothUUID("ee01"));
    multiple_non_matching_filter[1].services.push_back(BluetoothUUID("ee02"));
    multiple_non_matching_filter[1].services.push_back(BluetoothUUID("0003"));
    EXPECT_FALSE(list_.IsExcluded(multiple_non_matching_filter));
  }
}

TEST_F(BluetoothBlacklistTest, IsExcluded_BluetoothScanFilter_ReturnsTrue) {
  list_.Add(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  {
    std::vector<BluetoothScanFilter> single_matching_filter(1);
    single_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(list_.IsExcluded(single_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> first_matching_filter(2);
    first_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    first_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    first_matching_filter[1].services.push_back(BluetoothUUID("0002"));
    first_matching_filter[1].services.push_back(BluetoothUUID("0003"));
    EXPECT_TRUE(list_.IsExcluded(first_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> last_matching_filter(2);
    last_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    last_matching_filter[0].services.push_back(BluetoothUUID("0001"));
    last_matching_filter[1].services.push_back(BluetoothUUID("0002"));
    last_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(list_.IsExcluded(last_matching_filter));
  }
  {
    std::vector<BluetoothScanFilter> multiple_matching_filter(2);
    multiple_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[0].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    multiple_matching_filter[1].services.push_back(BluetoothUUID("eeee"));
    EXPECT_TRUE(list_.IsExcluded(multiple_matching_filter));
  }
}

TEST_F(BluetoothBlacklistTest, RemoveExcludedUuids_NonMatching) {
  list_.Add(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("ee01"), BluetoothBlacklist::Value::EXCLUDE_READS);
  list_.Add(BluetoothUUID("ee02"), BluetoothBlacklist::Value::EXCLUDE_WRITES);
  {
    std::vector<BluetoothUUID> empty;
    std::vector<BluetoothUUID> expected_empty;
    list_.RemoveExcludedUuids(&empty);
    EXPECT_EQ(expected_empty, empty);
  }
  {
    std::vector<BluetoothUUID> single_non_matching;
    single_non_matching.push_back(BluetoothUUID("0000"));

    std::vector<BluetoothUUID> expected_copy(single_non_matching);

    list_.RemoveExcludedUuids(&single_non_matching);
    EXPECT_EQ(expected_copy, single_non_matching);
  }
  {
    std::vector<BluetoothUUID> multiple_non_matching;
    multiple_non_matching.push_back(BluetoothUUID("0000"));
    multiple_non_matching.push_back(BluetoothUUID("ee01"));
    multiple_non_matching.push_back(BluetoothUUID("ee02"));
    multiple_non_matching.push_back(BluetoothUUID("0003"));

    std::vector<BluetoothUUID> expected_copy(multiple_non_matching);

    list_.RemoveExcludedUuids(&multiple_non_matching);
    EXPECT_EQ(expected_copy, multiple_non_matching);
  }
}

TEST_F(BluetoothBlacklistTest, RemoveExcludedUuids_Matching) {
  list_.Add(BluetoothUUID("eeee"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("eee2"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("eee3"), BluetoothBlacklist::Value::EXCLUDE);
  list_.Add(BluetoothUUID("eee4"), BluetoothBlacklist::Value::EXCLUDE);
  {
    std::vector<BluetoothUUID> single_matching;
    single_matching.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected_empty;

    list_.RemoveExcludedUuids(&single_matching);
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

    list_.RemoveExcludedUuids(&single_matching_of_many);
    EXPECT_EQ(expected, single_matching_of_many);
  }
  {
    std::vector<BluetoothUUID> all_matching_of_many;
    all_matching_of_many.push_back(BluetoothUUID("eee2"));
    all_matching_of_many.push_back(BluetoothUUID("eee4"));
    all_matching_of_many.push_back(BluetoothUUID("eee3"));
    all_matching_of_many.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected_empty;

    list_.RemoveExcludedUuids(&all_matching_of_many);
    EXPECT_EQ(expected_empty, all_matching_of_many);
  }
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultBlacklistSize) {
  // When adding items to the blacklist the new values should be added in the
  // tests below for each exclusion type.
  EXPECT_EQ(11u, list_.size());
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeList) {
  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("1800")));
  EXPECT_FALSE(list_.IsExcluded(BluetoothUUID("1801")));
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("1812")));
  EXPECT_TRUE(
      list_.IsExcluded(BluetoothUUID("00001530-1212-efde-1523-785feabcd123")));
  EXPECT_TRUE(
      list_.IsExcluded(BluetoothUUID("f000ffc0-0451-4000-b000-000000000000")));
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_.IsExcluded(BluetoothUUID("2a25")));
  EXPECT_TRUE(
      list_.IsExcluded(BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeReadList) {
  EXPECT_FALSE(list_.IsExcludedFromReads(BluetoothUUID("1800")));
  EXPECT_FALSE(list_.IsExcludedFromReads(BluetoothUUID("1801")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("1812")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_.IsExcludedFromReads(BluetoothUUID("2a25")));
  EXPECT_TRUE(list_.IsExcludedFromReads(
      BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135")));
  EXPECT_TRUE(list_.IsExcludedFromReads(
      BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
  EXPECT_TRUE(list_.IsExcludedFromReads(
      BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114")));
}

TEST_F(BluetoothBlacklistTest, VerifyDefaultExcludeWriteList) {
  EXPECT_FALSE(list_.IsExcludedFromWrites(BluetoothUUID("1800")));
  EXPECT_FALSE(list_.IsExcludedFromWrites(BluetoothUUID("1801")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("1812")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("2a02")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("2a25")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("2902")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(BluetoothUUID("2903")));
  EXPECT_TRUE(list_.IsExcludedFromWrites(
      BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
}

}  // namespace content
