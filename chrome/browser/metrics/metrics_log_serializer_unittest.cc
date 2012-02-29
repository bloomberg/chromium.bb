// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/md5.h"
#include "base/values.h"
#include "chrome/browser/metrics/metrics_log_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef MetricsLogManager::SerializedLog SerializedLog;

namespace {

const size_t kMaxLocalListSize = 3;

}  // namespace

class MetricsLogSerializerTest : public ::testing::Test {
};

// Store and retrieve empty list.
TEST(MetricsLogSerializerTest, EmptyLogList) {
  ListValue list;
  std::vector<SerializedLog> local_list;

  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);
  EXPECT_EQ(0U, list.GetSize());

  local_list.clear();  // ReadLogsFromPrefList() expects empty |local_list|.
  EXPECT_EQ(
      MetricsLogSerializer::LIST_EMPTY,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
  EXPECT_EQ(0U, local_list.size());
}

// Store and retrieve a single log value.
TEST(MetricsLogSerializerTest, SingleElementLogList) {
  ListValue list;

  std::vector<SerializedLog> local_list(1);
  local_list[0].xml = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);

  // |list| will now contain the following:
  // [1, Base64Encode("Hello world!"), MD5("Hello world!")].
  ASSERT_EQ(3U, list.GetSize());

  // Examine each element.
  ListValue::const_iterator it = list.begin();
  int size = 0;
  (*it)->GetAsInteger(&size);
  EXPECT_EQ(1, size);

  ++it;
  std::string str;
  (*it)->GetAsString(&str);  // Base64 encoded "Hello world!" string.
  std::string encoded;
  base::Base64Encode("Hello world!", &encoded);
  EXPECT_TRUE(encoded == str);

  ++it;
  (*it)->GetAsString(&str);  // MD5 for encoded "Hello world!" string.
  EXPECT_TRUE(base::MD5String(encoded) == str);

  ++it;
  EXPECT_TRUE(it == list.end());  // Reached end of list.

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
  EXPECT_EQ(1U, local_list.size());
}

// Store elements greater than the limit.
TEST(MetricsLogSerializerTest, OverLimitLogList) {
  ListValue list;

  std::vector<SerializedLog> local_list(4);
  local_list[0].proto = "one";
  local_list[1].proto = "two";
  local_list[2].proto = "three";
  local_list[3].proto = "four";

  std::string expected_first;
  base::Base64Encode(local_list[local_list.size() - kMaxLocalListSize].proto,
                     &expected_first);
  std::string expected_last;
  base::Base64Encode(local_list[local_list.size() - 1].proto,
                     &expected_last);

  MetricsLogSerializer::WriteLogsToPrefList(local_list, false,
                                            kMaxLocalListSize, &list);
  EXPECT_EQ(kMaxLocalListSize + 2, list.GetSize());

  std::string actual_first;
  EXPECT_TRUE((*(list.begin() + 1))->GetAsString(&actual_first));
  EXPECT_EQ(expected_first, actual_first);

  std::string actual_last;
  EXPECT_TRUE((*(list.end() - 2))->GetAsString(&actual_last));
  EXPECT_EQ(expected_last, actual_last);

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
  EXPECT_EQ(kMaxLocalListSize, local_list.size());
}

// Induce LIST_SIZE_TOO_SMALL corruption
TEST(MetricsLogSerializerTest, SmallRecoveredListSize) {
  ListValue list;

  std::vector<SerializedLog> local_list(1);
  local_list[0].xml = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);
  EXPECT_EQ(3U, list.GetSize());

  // Remove last element.
  list.Remove(list.GetSize() - 1, NULL);
  EXPECT_EQ(2U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_TOO_SMALL,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
}

// Remove size from the stored list.
TEST(MetricsLogSerializerTest, RemoveSizeFromLogList) {
  ListValue list;

  std::vector<SerializedLog> local_list(2);
  local_list[0].xml = "one";
  local_list[1].xml = "two";
  EXPECT_EQ(2U, local_list.size());
  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);
  EXPECT_EQ(4U, list.GetSize());

  list.Remove(0, NULL);  // Delete size (1st element).
  EXPECT_EQ(3U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_MISSING,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
}

// Corrupt size of stored list.
TEST(MetricsLogSerializerTest, CorruptSizeOfLogList) {
  ListValue list;

  std::vector<SerializedLog> local_list(1);
  local_list[0].xml = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);
  EXPECT_EQ(3U, list.GetSize());

  // Change list size from 1 to 2.
  EXPECT_TRUE(list.Set(0, Value::CreateIntegerValue(2)));
  EXPECT_EQ(3U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_CORRUPTION,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
}

// Corrupt checksum of stored list.
TEST(MetricsLogSerializerTest, CorruptChecksumOfLogList) {
  ListValue list;

  std::vector<SerializedLog> local_list(1);
  local_list[0].xml = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, true, kMaxLocalListSize,
                                            &list);
  EXPECT_EQ(3U, list.GetSize());

  // Fetch checksum (last element) and change it.
  std::string checksum;
  EXPECT_TRUE((*(list.end() - 1))->GetAsString(&checksum));
  checksum[0] = (checksum[0] == 'a') ? 'b' : 'a';
  EXPECT_TRUE(list.Set(2, Value::CreateStringValue(checksum)));
  EXPECT_EQ(3U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::CHECKSUM_CORRUPTION,
      MetricsLogSerializer::ReadLogsFromPrefList(list, true, &local_list));
}
