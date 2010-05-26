// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a file for random testcases that we run into that at one point or
// another have crashed the program.

#include "chrome/test/ui/ui_test.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_thread.h"
#include "chrome/common/chrome_switches.h"

class GoogleTest : public UITest {
 protected:
  GoogleTest() : UITest() {
    FilePath test_file(test_data_directory_);
    test_file = test_file.AppendASCII("google");
    test_file = test_file.AppendASCII("google.html");
    homepage_ = test_file.ToWStringHack();
  }
};

TEST_F(GoogleTest, Crash) {
  std::wstring page_title = L"Google";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}

class ColumnLayout : public UITest {
 protected:
  ColumnLayout() : UITest() {
    FilePath test_file(test_data_directory_);
    test_file = test_file.AppendASCII("columns.html");
    homepage_ = test_file.ToWStringHack();
  }
};

TEST_F(ColumnLayout, Crash) {
  std::wstring page_title = L"Column test";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}

// By passing kTryChromeAgain with a magic value > 10000 we cause chrome
// to exit fairly early. This was the cause of crashes. See bug 34799.
class EarlyReturnTest : public UITest {
 public:
  EarlyReturnTest() {
    wait_for_initial_loads_ = false;
    // We don't depend on these timeouts, they are set to the minimum so
    // the automation server waits the minimun amount possible for the
    // handshake that will never come.
    set_command_execution_timeout_ms(1);
    set_action_timeout_ms(1);
    launch_arguments_.AppendSwitchWithValue(switches::kTryChromeAgain, "10001");
  }
};

// Flaky: http://crbug.com/45115
TEST_F(EarlyReturnTest, FLAKY_ToastCrasher) {
  // UITest will check if this crashed.
}
