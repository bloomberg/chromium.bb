// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/upload_list.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that UploadList can parse a vector of log entry strings to a vector of
// UploadInfo objects. See the UploadList declaration for a description of the
// log entry string format.
TEST(UploadListTest, ParseLogEntries) {
  const char kTestTime[] = "1234567890";
  const char kTestID[] = "0123456789abcdef";
  std::string test_entry = kTestTime;
  test_entry += ",";
  test_entry.append(kTestID, sizeof(kTestID));

  scoped_refptr<UploadList> upload_list =
      new UploadList(NULL, base::FilePath());

  // 1 entry.
  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].time.ToDoubleT();
  EXPECT_STREQ(kTestTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestID, upload_list->uploads_[0].id.c_str());
  EXPECT_STREQ("", upload_list->uploads_[0].local_id.c_str());

  // Add 3 more entries.
  log_entries.push_back(test_entry);
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(4u, upload_list->uploads_.size());
  time_double = upload_list->uploads_[3].time.ToDoubleT();
  EXPECT_STREQ(kTestTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestID, upload_list->uploads_[3].id.c_str());
  EXPECT_STREQ("", upload_list->uploads_[3].local_id.c_str());
}

TEST(UploadListTest, ParseLogEntriesWithLocalId) {
  const char kTestTime[] = "1234567890";
  const char kTestID[] = "0123456789abcdef";
  const char kTestLocalID[] = "fedcba9876543210";
  std::string test_entry = kTestTime;
  test_entry += ",";
  test_entry.append(kTestID, sizeof(kTestID));
  test_entry += ",";
  test_entry.append(kTestLocalID, sizeof(kTestLocalID));

  scoped_refptr<UploadList> upload_list =
      new UploadList(NULL, base::FilePath());

  // 1 entry.
  std::vector<std::string> log_entries;
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(1u, upload_list->uploads_.size());
  double time_double = upload_list->uploads_[0].time.ToDoubleT();
  EXPECT_STREQ(kTestTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestID, upload_list->uploads_[0].id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[0].local_id.c_str());

  // Add 3 more entries.
  log_entries.push_back(test_entry);
  log_entries.push_back(test_entry);
  upload_list->ParseLogEntries(log_entries);
  EXPECT_EQ(4u, upload_list->uploads_.size());
  time_double = upload_list->uploads_[3].time.ToDoubleT();
  EXPECT_STREQ(kTestTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestID, upload_list->uploads_[3].id.c_str());
  EXPECT_STREQ(kTestLocalID, upload_list->uploads_[3].local_id.c_str());
}
