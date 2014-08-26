// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc_log_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kExpectedDaysToKeepLogFiles = 5;

class WebRtcLogUtilTest : public testing::Test {
 public:
  WebRtcLogUtilTest()
      : file_thread_(content::BrowserThread::FILE, &message_loop_) {}

  virtual void SetUp() {
    // Create three files. One with modified date as of now, one with date one
    // day younger than the keep limit, one with date one day older than the
    // limit. The two former are expected to be kept and the last to be deleted
    // when deleting old logs.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath file;
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.path(), &file));
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.path(), &file));
    base::Time time_expect_to_keep =
        base::Time::Now() -
        base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles - 1);
    TouchFile(file, time_expect_to_keep, time_expect_to_keep);
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.path(), &file));
    base::Time time_expect_to_delete =
        base::Time::Now() -
        base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles + 1);
    TouchFile(file, time_expect_to_delete, time_expect_to_delete);
  }

  void VerifyFiles(int expected_files) {
    base::FileEnumerator files(dir_.path(), false, base::FileEnumerator::FILES);
    int file_counter = 0;
    for (base::FilePath name = files.Next(); !name.empty();
         name = files.Next()) {
      EXPECT_LT(base::Time::Now() - files.GetInfo().GetLastModifiedTime(),
                base::TimeDelta::FromDays(kExpectedDaysToKeepLogFiles));
      ++file_counter;
    }
    EXPECT_EQ(expected_files, file_counter);
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread file_thread_;
  base::ScopedTempDir dir_;
};

TEST_F(WebRtcLogUtilTest, DeleteOldWebRtcLogFiles) {
  WebRtcLogUtil::DeleteOldWebRtcLogFiles(dir_.path());
  VerifyFiles(2);
}

TEST_F(WebRtcLogUtilTest, DeleteOldAndRecentWebRtcLogFiles) {
  base::Time time_begin_delete =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles(dir_.path(),
                                                  time_begin_delete);
  VerifyFiles(1);
}
