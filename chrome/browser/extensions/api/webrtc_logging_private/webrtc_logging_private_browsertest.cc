// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/webrtc_logging/browser/text_log_list.h"

class WebrtcLoggingPrivateApiBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  WebrtcLoggingPrivateApiBrowserTest() = default;
  ~WebrtcLoggingPrivateApiBrowserTest() override = default;

  base::FilePath webrtc_logs_path() {
    return webrtc_logging::TextLogList::
        GetWebRtcLogDirectoryForBrowserContextPath(profile()->GetPath());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebrtcLoggingPrivateApiBrowserTest);
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestNoGetLogsDirectoryPermissionsFromHangoutsExtension) {
  ASSERT_TRUE(RunComponentExtensionTest(
      "api_test/webrtc_logging_private/no_get_logs_directory_permissions"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiBrowserTest,
                       TestStartAudioDebugRecordingsForWebviewFromApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/webrtc_logging_private/audio_debug/"
                         "start_audio_debug_recordings_for_webview_from_app"))
      << message_;
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiBrowserTest,
    TestStartAudioDebugRecordingsForWebviewFromAppWithoutSwitch) {
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/webrtc_logging_private/audio_debug/"
                         "start_audio_debug_recordings_for_webview_from_app"))
      << message_;
}
#endif
