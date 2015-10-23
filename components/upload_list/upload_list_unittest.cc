// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/upload_list.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUploadTime[] = "1234567890";
const char kTestUploadId[] = "0123456789abcdef";
const char kTestLocalID[] = "fedcba9876543210";
const char kTestCaptureTime[] = "2345678901";

}  // namespace

// These tests test that UploadList can parse a vector of log entry strings of
// various formats to a vector of UploadInfo objects. See the UploadList
// declaration for a description of the log entry string formats.

// Test log entry string with upload time and upload ID.
// This is the format that crash reports are stored in.
TEST(UploadListTest, ParseUploadTimeUploadId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);

  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ("", upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and local ID.
// This is the old format that WebRTC logs were stored in.
TEST(UploadListTest, ParseUploadTimeUploadIdLocalId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);

  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and capture time.
// This is the format that WebRTC logs that only have been uploaded only are
// stored in.
TEST(UploadListTest, ParseUploadTimeUploadIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",,";
  test_entry.append(kTestCaptureTime);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);

  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ("", upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

// Test log entry string with local ID and capture time.
// This is the format that WebRTC logs that only are stored locally are stored
// in.
TEST(UploadListTest, ParseLocalIdCaptureTime) {
  std::string test_entry = ",,";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);

  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
  EXPECT_STREQ("", upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID, local ID and capture
// time.
// This is the format that WebRTC logs that are stored locally and have been
// uploaded are stored in.
TEST(UploadListTest, ParseUploadTimeUploadIdLocalIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);

  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

TEST(UploadListTest, ParseMultipleEntries) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);

  scoped_refptr<UploadList> upload_list =
      new UploadList(nullptr, base::FilePath(), nullptr);

  // 1 entry.
  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, upload_list->uploads_[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[0].local_id.c_str());
  time_double = upload_list->uploads_[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());

  // Add 3 more entries.
  log_entries.push_back(test_entry);
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(4u, upload_list->uploads_.size());
  for (size_t i = 0; i < upload_list->uploads_.size(); ++i) {
    time_double = upload_list->uploads_[i].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload_list->uploads_[i].upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload_list->uploads_[i].local_id.c_str());
    time_double = upload_list->uploads_[i].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
  }
}
