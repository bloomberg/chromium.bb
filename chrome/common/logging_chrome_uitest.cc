// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLoggingTest : public testing::Test {
 public:
  // Stores the current value of the log file name environment
  // variable and sets the variable to new_value.
  void SaveEnvironmentVariable(std::string new_value) {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (!env->GetVar(env_vars::kLogFileName, &environment_filename_))
      environment_filename_ = "";

    env->SetVar(env_vars::kLogFileName, new_value);
  }

  // Restores the value of the log file nave environment variable
  // previously saved by SaveEnvironmentVariable().
  void RestoreEnvironmentVariable() {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar(env_vars::kLogFileName, environment_filename_);
  }

 private:
  std::string environment_filename_;  // Saves real environment value.
};

// Tests the log file name getter without an environment variable.
TEST_F(ChromeLoggingTest, LogFileName) {
  SaveEnvironmentVariable("");

  FilePath filename = logging::GetLogFileName();
  ASSERT_NE(FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("chrome_debug.log")));

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("test value");

  FilePath filename = logging::GetLogFileName();
  ASSERT_EQ(FilePath(FILE_PATH_LITERAL("test value")).value(),
            filename.value());

  RestoreEnvironmentVariable();
}

#if defined(OS_LINUX) && (!defined(NDEBUG) || !defined(USE_LINUX_BREAKPAD))
// On Linux in Debug mode, Chrome generates a SIGTRAP.
// we do not catch SIGTRAPs, thus no crash dump.
// This also does not work if Breakpad is disabled.
#define EXPECTED_ASSERT_CRASHES 0
#else
#define EXPECTED_ASSERT_CRASHES 1
#endif

// Touch build will start an extra renderer process (the extension process)
// for the virtual keyboard.
#if defined(TOUCH_UI)
#define EXPECTED_ASSERT_ERRORS 2
#else
#define EXPECTED_ASSERT_ERRORS 1
#endif

#if !defined(NDEBUG)  // We don't have assertions in release builds.
// Tests whether we correctly fail on renderer assertions during tests.
class AssertionTest : public UITest {
 protected:
  AssertionTest() {
#if defined(OS_WIN)
    // TODO(phajdan.jr): Make crash notifications on launch work on Win.
    wait_for_initial_loads_ = false;
#endif
    launch_arguments_.AppendSwitch(switches::kRendererAssertTest);
  }
};

// Launch the app in assertion test mode, then close the app.
#if defined(OS_WIN)
// http://crbug.com/26715
#define Assertion DISABLED_Assertion
#elif defined(OS_MACOSX)
// Crash service doesn't exist for the Mac yet: http://crbug.com/45243
#define Assertion DISABLED_Assertion
#endif
TEST_F(AssertionTest, Assertion) {
  expected_errors_ = EXPECTED_ASSERT_ERRORS;
  expected_crashes_ = EXPECTED_ASSERT_CRASHES;
}
#endif  // !defined(NDEBUG)

#if !defined(OFFICIAL_BUILD)
// Only works on Linux in Release mode with CHROME_HEADLESS=1
class CheckFalseTest : public UITest {
 protected:
  CheckFalseTest() {
#if defined(OS_WIN)
    // TODO(phajdan.jr): Make crash notifications on launch work on Win.
    wait_for_initial_loads_ = false;
#endif
    launch_arguments_.AppendSwitch(switches::kRendererCheckFalseTest);
  }
};

#if defined(OS_WIN)
// http://crbug.com/38497
#define CheckFails FLAKY_CheckFails
#elif defined(OS_MACOSX)
// Crash service doesn't exist for the Mac yet: http://crbug.com/45243
#define CheckFails DISABLED_CheckFails
#endif
// Launch the app in assertion test mode, then close the app.
TEST_F(CheckFalseTest, CheckFails) {
  expected_errors_ = EXPECTED_ASSERT_ERRORS;
  expected_crashes_ = EXPECTED_ASSERT_CRASHES;
}
#endif  // !defined(OFFICIAL_BUILD)

// Tests whether we correctly fail on browser crashes during UI Tests.
class RendererCrashTest : public UITest {
 protected:
  RendererCrashTest() {
#if defined(OS_WIN)
    // TODO(phajdan.jr): Make crash notifications on launch work on Win.
    wait_for_initial_loads_ = false;
#endif
    launch_arguments_.AppendSwitch(switches::kRendererCrashTest);
  }
};

#if defined(OS_LINUX) && !defined(USE_LINUX_BREAKPAD)
// On Linux, do not expect a crash dump if Breakpad is disabled.
#define EXPECTED_CRASH_CRASHES 0
#else
#define EXPECTED_CRASH_CRASHES 1
#endif

#if defined(OS_MACOSX)
// Crash service doesn't exist for the Mac yet: http://crbug.com/45243
#define Crash DISABLED_Crash
#endif
// Launch the app in renderer crash test mode, then close the app.
TEST_F(RendererCrashTest, Crash) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->WaitForTabCountToBecome(1));
  expected_crashes_ = EXPECTED_CRASH_CRASHES;
}
