// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/media/webrtc/webrtc_log_list.h"

class WebrtcLoggingPrivateApiBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  WebrtcLoggingPrivateApiBrowserTest() = default;
  ~WebrtcLoggingPrivateApiBrowserTest() override = default;

  base::FilePath webrtc_logs_path() {
    return WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile()->GetPath());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebrtcLoggingPrivateApiBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestGetLogsDirectoryCreatesWebRtcLogsDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(webrtc_logs_path()));
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "api_test/webrtc_logging_private/get_logs_directory",
      "test_without_directory"))
      << message_;
  ASSERT_TRUE(base::PathExists(webrtc_logs_path()));
  ASSERT_TRUE(base::IsDirectoryEmpty(webrtc_logs_path()));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestGetLogsDirectoryReadsFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(webrtc_logs_path()));
  base::FilePath test_file_path = webrtc_logs_path().AppendASCII("test.file");
  std::string contents = "test file contents";
  ASSERT_EQ(base::checked_cast<int>(contents.size()),
            base::WriteFile(test_file_path, contents.c_str(), contents.size()));
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "api_test/webrtc_logging_private/get_logs_directory",
      "test_with_file_in_directory"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestNoGetLogsDirectoryPermissionsFromHangoutsExtension) {
  ASSERT_TRUE(RunComponentExtensionTest(
      "api_test/webrtc_logging_private/no_get_logs_directory_permissions"))
      << message_;
}
