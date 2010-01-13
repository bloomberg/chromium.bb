// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include "chrome/test/ui/ui_test.h"

class ChromeProcessUtilTest : public UITest {
};

TEST_F(ChromeProcessUtilTest, SanityTest) {
  EXPECT_TRUE(IsBrowserRunning());
  ChromeProcessList processes = GetRunningChromeProcesses(browser_process_id());
  EXPECT_FALSE(processes.empty());
  QuitBrowser();
  EXPECT_FALSE(IsBrowserRunning());
  EXPECT_EQ(-1, browser_process_id());
  processes = GetRunningChromeProcesses(browser_process_id());
  EXPECT_TRUE(processes.empty());
}
