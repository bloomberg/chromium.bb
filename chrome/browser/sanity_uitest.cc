// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Flakily fails under Valgrind, see http://crbug.com/85863.
#if defined(OS_MACOSX)
#define MAYBE_Crash FLAKY_Crash
#else
#define MAYBE_Crash Crash
#endif  // defined(OS_MACOSX)
TEST_F(GoogleTest, MAYBE_Crash) {
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

// Flakily fails under Valgrind, see http://crbug.com/85863.
TEST_F(ColumnLayout, MAYBE_Crash) {
  std::wstring page_title = L"Column test";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}
