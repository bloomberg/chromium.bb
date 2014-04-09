// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the --load-and-launch-app switch.
// The two cases are when chrome is running and another process uses the switch
// and when chrome is started from scratch.

#include "apps/switches.h"
#include "base/process/launch.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_launcher.h"

using extensions::PlatformAppBrowserTest;

namespace apps {

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465
#if defined(OS_MACOSX)
#define MAYBE_LoadAndLaunchAppChromeRunning \
        DISABLED_LoadAndLaunchAppChromeRunning
#else
#define MAYBE_LoadAndLaunchAppChromeRunning LoadAndLaunchAppChromeRunning
#endif

// Case where Chrome is already running.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppChromeRunning) {
  ExtensionTestMessageListener launched_listener("Launched", false);

  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine new_cmdline(cmdline.GetProgram());

  const char* kSwitchNames[] = {
    switches::kUserDataDir,
  };
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchNames, arraysize(kSwitchNames));

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("minimal");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());

  new_cmdline.AppendSwitch(content::kLaunchAsBrowser);
  base::ProcessHandle process;
  base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest(), &process);
  ASSERT_NE(base::kNullProcessHandle, process);

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_TRUE(base::WaitForSingleProcess(
      process, TestTimeouts::action_timeout()));
}

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465
#if defined(OS_MACOSX)
#define MAYBE_LoadAndLaunchAppWithFile DISABLED_LoadAndLaunchAppWithFile
#else
#define MAYBE_LoadAndLaunchAppWithFile LoadAndLaunchAppWithFile
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppWithFile) {
  ExtensionTestMessageListener launched_listener("Launched", false);

  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine new_cmdline(cmdline.GetProgram());

  const char* kSwitchNames[] = {
    switches::kUserDataDir,
  };
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchNames, arraysize(kSwitchNames));

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("load_and_launch_file");

  base::FilePath test_file_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("launch_files")
      .AppendASCII("test.txt");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());
  new_cmdline.AppendSwitch(content::kLaunchAsBrowser);
  new_cmdline.AppendArgPath(test_file_path);

  base::ProcessHandle process;
  base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest(), &process);
  ASSERT_NE(base::kNullProcessHandle, process);

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_TRUE(base::WaitForSingleProcess(
      process, TestTimeouts::action_timeout()));
}

namespace {

// TestFixture that appends --load-and-launch-app before calling BrowserMain.
class PlatformAppLoadAndLaunchBrowserTest : public PlatformAppBrowserTest {
 protected:
  PlatformAppLoadAndLaunchBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    app_path_ = test_data_dir_
        .AppendASCII("platform_apps")
        .AppendASCII("minimal");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp,
                                     app_path_.value());
  }

  void LoadAndLaunchApp() {
    ExtensionTestMessageListener launched_listener("Launched", false);
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    // Start an actual browser because we can't shut down with just an app
    // window.
    CreateBrowser(ProfileManager::GetActiveUserProfile());
  }

 private:
  base::FilePath app_path_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppLoadAndLaunchBrowserTest);
};

}  // namespace


// TODO(jackhou): Make this test not flaky on Vista or Linux Aura. See
// http://crbug.com/176897
#if defined(OS_WIN) || (defined(OS_LINUX) && defined(USE_AURA))
#define MAYBE_LoadAndLaunchAppChromeNotRunning \
        DISABLED_LoadAndLaunchAppChromeNotRunning
#else
#define MAYBE_LoadAndLaunchAppChromeNotRunning \
        LoadAndLaunchAppChromeNotRunning
#endif

// Case where Chrome is not running.
IN_PROC_BROWSER_TEST_F(PlatformAppLoadAndLaunchBrowserTest,
                       MAYBE_LoadAndLaunchAppChromeNotRunning) {
  LoadAndLaunchApp();
}

}  // namespace apps
