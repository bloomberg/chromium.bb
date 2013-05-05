// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/md5.h"
#include "base/values.h"
#include "chrome/browser/metrics/metrics_log_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kListLengthLimit = 3;
const size_t kLogByteLimit = 1000;

}  // namespace

class MetricsLogSerializerTest : public ::testing::Test {
};

// Store and retrieve empty list.
TEST(MetricsLogSerializerTest, EmptyLogList) {
  ListValue list;
  std::vector<std::string> local_list;

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  EXPECT_EQ(0U, list.GetSize());

  local_list.clear();  // ReadLogsFromPrefList() expects empty |local_list|.
  EXPECT_EQ(
      MetricsLogSerializer::LIST_EMPTY,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
  EXPECT_EQ(0U, local_list.size());
}

// Store and retrieve a single log value.
TEST(MetricsLogSerializerTest, SingleElementLogList) {
  ListValue list;

  std::vector<std::string> local_list(1);
  local_list[0] = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);

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
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
  EXPECT_EQ(1U, local_list.size());
}

// Store a set of logs over the length limit, but smaller than the min number of
// bytes.
TEST(MetricsLogSerializerTest, LongButTinyLogList) {
  ListValue list;

  size_t log_count = kListLengthLimit * 5;
  std::vector<std::string> local_list(log_count);
  for (size_t i = 0; i < local_list.size(); ++i) {
    local_list[0] = "x";
  }

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  std::vector<std::string> result_list;
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &result_list));
  EXPECT_EQ(local_list.size(), result_list.size());

  EXPECT_TRUE(result_list.front().find("x") == 0);
}

// Store a set of logs over the length limit, but that doesn't reach the minimum
// number of bytes until after passing the length limit.
TEST(MetricsLogSerializerTest, LongButSmallLogList) {
  ListValue list;

  size_t log_count = kListLengthLimit * 5;
  // Make log_count logs each slightly larger than
  // kLogByteLimit / (log_count - 2)
  // so that the minimum is reached before the oldest (first) two logs.
  std::vector<std::string> local_list(log_count);
  size_t log_size = (kLogByteLimit / (log_count - 2)) + 2;
  local_list[0] = "one";
  local_list[1] = "two";
  local_list[2] = "three";
  local_list[log_count - 1] = "last";
  for (size_t i = 0; i < local_list.size(); ++i) {
    local_list[i].resize(log_size, ' ');
  }

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  std::vector<std::string> result_list;
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &result_list));
  EXPECT_EQ(local_list.size() - 2, result_list.size());

  EXPECT_TRUE(result_list.front().find("three") == 0);
  EXPECT_TRUE(result_list.back().find("last") == 0);
}

// Store a set of logs within the length limit, but well over the minimum
// number of bytes.
TEST(MetricsLogSerializerTest, ShortButLargeLogList) {
  ListValue list;

  std::vector<std::string> local_list(kListLengthLimit);
  // Make the total byte count about twice the minimum.
  size_t log_size = (kLogByteLimit / local_list.size()) * 2;
  for (size_t i = 0; i < local_list.size(); ++i) {
    local_list[i].resize(log_size, ' ');
  }

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  std::vector<std::string> result_list;
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &result_list));
  EXPECT_EQ(local_list.size(), result_list.size());
}

// Store a set of logs over the length limit, and over the minimum number of
// bytes.
TEST(MetricsLogSerializerTest, LongAndLargeLogList) {
  ListValue list;

  // Include twice the max number of logs.
  std::vector<std::string> local_list(kListLengthLimit * 2);
  // Make the total byte count about four times the minimum.
  size_t log_size = (kLogByteLimit / local_list.size()) * 4;
  local_list[local_list.size() - kListLengthLimit] = "First to keep";
  for (size_t i = 0; i < local_list.size(); ++i) {
    local_list[i].resize(log_size, ' ');
  }

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  std::vector<std::string> result_list;
  EXPECT_EQ(
      MetricsLogSerializer::RECALL_SUCCESS,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &result_list));
  // The max length should control the resulting size.
  EXPECT_EQ(kListLengthLimit, result_list.size());
  EXPECT_TRUE(result_list.front().find("First to keep") == 0);
}

// Induce LIST_SIZE_TOO_SMALL corruption
TEST(MetricsLogSerializerTest, SmallRecoveredListSize) {
  ListValue list;

  std::vector<std::string> local_list(1);
  local_list[0] = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  EXPECT_EQ(3U, list.GetSize());

  // Remove last element.
  list.Remove(list.GetSize() - 1, NULL);
  EXPECT_EQ(2U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_TOO_SMALL,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
}

// Remove size from the stored list.
TEST(MetricsLogSerializerTest, RemoveSizeFromLogList) {
  ListValue list;

  std::vector<std::string> local_list(2);
  local_list[0] = "one";
  local_list[1] = "two";
  EXPECT_EQ(2U, local_list.size());
  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  EXPECT_EQ(4U, list.GetSize());

  list.Remove(0, NULL);  // Delete size (1st element).
  EXPECT_EQ(3U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_MISSING,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
}

// Corrupt size of stored list.
TEST(MetricsLogSerializerTest, CorruptSizeOfLogList) {
  ListValue list;

  std::vector<std::string> local_list(1);
  local_list[0] = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
  EXPECT_EQ(3U, list.GetSize());

  // Change list size from 1 to 2.
  EXPECT_TRUE(list.Set(0, Value::CreateIntegerValue(2)));
  EXPECT_EQ(3U, list.GetSize());

  local_list.clear();
  EXPECT_EQ(
      MetricsLogSerializer::LIST_SIZE_CORRUPTION,
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
}

// Corrupt checksum of stored list.
TEST(MetricsLogSerializerTest, CorruptChecksumOfLogList) {
  ListValue list;

  std::vector<std::string> local_list(1);
  local_list[0] = "Hello world!";

  MetricsLogSerializer::WriteLogsToPrefList(local_list, kListLengthLimit,
                                            kLogByteLimit, &list);
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
      MetricsLogSerializer::ReadLogsFromPrefList(list, &local_list));
}
