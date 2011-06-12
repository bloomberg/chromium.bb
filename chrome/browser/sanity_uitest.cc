// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a file for random testcases that we run into that at one point or
// another have crashed the program.

#include "chrome/test/ui/ui_test.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/net_util.h"

class GoogleTest : public UITest {
 protected:
  GoogleTest() : UITest() {
    FilePath test_file =
        test_data_directory_.AppendASCII("google").AppendASCII("google.html");
    set_homepage(GURL(net::FilePathToFileURL(test_file)).spec());
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
    FilePath test_file = test_data_directory_.AppendASCII("columns.html");
    set_homepage(GURL(net::FilePathToFileURL(test_file)).spec());
  }
};

TEST_F(ColumnLayout, Crash) {
  std::wstring page_title = L"Column test";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}

// By passing kTryChromeAgain with a magic value > 10000 we cause Chrome
// to exit fairly early.
// Quickly exiting Chrome (regardless of this particular flag -- it
// doesn't do anything other than cause Chrome to quit on startup on
// non-Windows) was a cause of crashes (see bug 34799 for example) so
// this is a useful test of the startup/quick-shutdown cycle.
class EarlyReturnTest : public UITest {
 public:
  EarlyReturnTest() {
    wait_for_initial_loads_ = false;  // Don't wait for any pages to load.
    launch_arguments_.AppendSwitchASCII(switches::kTryChromeAgain, "10001");
  }
};

// Disabled: http://crbug.com/45115
// Due to limitations in our test infrastructure, this test currently doesn't
// work.
TEST_F(EarlyReturnTest, DISABLED_ToastCrasher) {
  // UITest will check if this crashed.
}
