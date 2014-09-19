// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/result_codes.h"

// These tests don't apply to the Mac version; see GetCommandLineForRelaunch
// for details.
#if defined(OS_MACOSX)
#error This test file should not be part of the Mac build.
#endif

namespace {

class CloudPrintPolicyTest : public InProcessBrowserTest {
 public:
  CloudPrintPolicyTest() {}
};

IN_PROC_BROWSER_TEST_F(CloudPrintPolicyTest, NormalPassedFlag) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  base::FilePath test_file_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("empty.html"));
  CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  base::ProcessHandle handle;
  bool launched =
      base::LaunchProcess(new_command_line, base::LaunchOptionsForTest(),
          &handle);
  EXPECT_TRUE(launched);

  observer.Wait();

  int exit_code = -100;
  bool exited =
      base::WaitForExitCodeWithTimeout(handle, &exit_code,
                                       TestTimeouts::action_timeout());

  EXPECT_TRUE(exited);
  EXPECT_EQ(chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED, exit_code);
  base::CloseProcessHandle(handle);
}

// Disabled due to http://crbug.com/144393.
IN_PROC_BROWSER_TEST_F(CloudPrintPolicyTest, DISABLED_CloudPrintPolicyFlag) {
  CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendSwitch(switches::kCheckCloudPrintConnectorPolicy);
  // This is important for the test as the way the browser process is launched
  // here causes the predictor databases to be initialized multiple times. This
  // is not an issue for production where the process is launched as a service
  // and a Profile is not created. See http://crbug.com/140466 for more details.
  new_command_line.AppendSwitchASCII(
      switches::kSpeculativeResourcePrefetching,
      switches::kSpeculativeResourcePrefetchingDisabled);

  base::ProcessHandle handle;
  bool launched =
      base::LaunchProcess(new_command_line, base::LaunchOptionsForTest(),
          &handle);
  EXPECT_TRUE(launched);

  int exit_code = -100;
  bool exited =
      base::WaitForExitCodeWithTimeout(handle, &exit_code,
                                       TestTimeouts::action_timeout());

  EXPECT_TRUE(exited);
  EXPECT_EQ(content::RESULT_CODE_NORMAL_EXIT, exit_code);
  base::CloseProcessHandle(handle);
}

}  // namespace
