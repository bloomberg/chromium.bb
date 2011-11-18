// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "net/base/net_util.h"

typedef UITest ChromeMainTest;

#if !defined(OS_MACOSX)

#if defined(USE_AURA)
// http://crbug.com/104650
#define MAYBE_SecondLaunch FAILS_SecondLaunch
#define MAYBE_ReuseBrowserInstanceWhenOpeningFile \
        FAILS_ReuseBrowserInstanceWhenOpeningFile
#define MAYBE_SecondLaunchWithIncognitoUrl FAILS_SecondLaunchWithIncognitoUrl
#define MAYBE_SecondLaunchFromIncognitoWithNormalUrl \
        FAILS_SecondLaunchFromIncognitoWithNormalUrl
#else
#define MAYBE_SecondLaunch SecondLaunch
#define MAYBE_ReuseBrowserInstanceWhenOpeningFile \
        ReuseBrowserInstanceWhenOpeningFile
#define MAYBE_SecondLaunchWithIncognitoUrl SecondLaunchWithIncognitoUrl
#define MAYBE_SecondLaunchFromIncognitoWithNormalUrl \
        SecondLaunchFromIncognitoWithNormalUrl
#endif

// These tests don't apply to the Mac version; see
// LaunchAnotherBrowserBlockUntilClosed for details.

// Make sure that the second invocation creates a new window.
TEST_F(ChromeMainTest, MAYBE_SecondLaunch) {
  include_testing_id_ = false;

  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(
                  CommandLine(CommandLine::NO_PROGRAM)));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));
}

TEST_F(ChromeMainTest, MAYBE_ReuseBrowserInstanceWhenOpeningFile) {
  include_testing_id_ = false;

  FilePath test_file = test_data_directory_.AppendASCII("empty.html");

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendArgPath(test_file);
  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(command_line));

  ASSERT_TRUE(automation()->IsURLDisplayed(net::FilePathToFileURL(test_file)));
}

TEST_F(ChromeMainTest, MAYBE_SecondLaunchWithIncognitoUrl) {
  include_testing_id_ = false;
  int num_normal_windows;
  // We should start with one normal window.
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(1, num_normal_windows);

  // Run with --incognito switch and an URL specified.
  FilePath test_file = test_data_directory_.AppendASCII("empty.html");
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kIncognito);
  command_line.AppendArgPath(test_file);
  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(command_line));

  // There should be one normal and one incognito window now.
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(1, num_normal_windows);
}

TEST_F(ChromeMainTest, MAYBE_SecondLaunchFromIncognitoWithNormalUrl) {
  include_testing_id_ = false;
  int num_normal_windows;
  // We should start with one normal window.
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(1, num_normal_windows);

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  // Create an incognito window.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(1, num_normal_windows);

  // Close the first window.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_CLOSE_WINDOW));
  browser_proxy = NULL;

  // There should only be the incognito window open now.
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(0, num_normal_windows);

  // Run with just an URL specified, no --incognito switch.
  FilePath test_file = test_data_directory_.AppendASCII("empty.html");
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendArgPath(test_file);
  ASSERT_TRUE(LaunchAnotherBrowserBlockUntilClosed(command_line));

  // There should be one normal and one incognito window now.
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));
  ASSERT_TRUE(automation()->GetNormalBrowserWindowCount(&num_normal_windows));
  ASSERT_EQ(1, num_normal_windows);
}

#endif  // !OS_MACOSX
