// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "net/base/net_util.h"

typedef UITest ChromeMainTest;

#if !defined(OS_MACOSX)
// These tests don't apply to the Mac version; see
// LaunchAnotherBrowserBlockUntilClosed for details.

// Make sure that the second invocation creates a new window.
TEST_F(ChromeMainTest, SecondLaunch) {
  include_testing_id_ = false;

  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(
                  CommandLine(CommandLine::NO_PROGRAM)));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));
}

TEST_F(ChromeMainTest, ReuseBrowserInstanceWhenOpeningFile) {
  include_testing_id_ = false;

  FilePath test_file = test_data_directory_.AppendASCII("empty.html");

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendArgPath(test_file);
  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(command_line));

  ASSERT_TRUE(automation()->IsURLDisplayed(net::FilePathToFileURL(test_file)));
}
#endif  // !OS_MACOSX
