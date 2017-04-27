// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kExpectedDaysToKeepLogFiles = 5;

class WebRtcLogUtilTest : public testing::Test {
 public:
  WebRtcLogUtilTest() = default;

  void SetUp() override {
    // Create three files. One with modified date as of now, one with date one
    // day younger than the keep limit, one with date one day older than the
    // limit. The two former are expected to be kept and the last to be deleted
    // when deleting old logs.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath file;
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_keep =
        base::Time::Now() -
        base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles - 1);
    TouchFile(file, time_expect_to_keep, time_expect_to_keep);
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_delete =
        base::Time::Now() -
        base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles + 1);
    TouchFile(file, time_expect_to_delete, time_expect_to_delete);
  }

  void VerifyFiles(int expected_files) {
    base::FileEnumerator files(dir_.GetPath(), false,
                               base::FileEnumerator::FILES);
    int file_counter = 0;
    for (base::FilePath name = files.Next(); !name.empty();
         name = files.Next()) {
      EXPECT_LT(base::Time::Now() - files.GetInfo().GetLastModifiedTime(),
                base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles));
      ++file_counter;
    }
    EXPECT_EQ(expected_files, file_counter);
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  base::ScopedTempDir dir_;
};

TEST_F(WebRtcLogUtilTest, DeleteOldWebRtcLogFiles) {
  WebRtcLogUtil::DeleteOldWebRtcLogFiles(dir_.GetPath());
  VerifyFiles(2);
}

TEST_F(WebRtcLogUtilTest, DeleteOldAndRecentWebRtcLogFiles) {
  base::Time time_begin_delete =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(),
                                                  time_begin_delete);
  VerifyFiles(1);
}
