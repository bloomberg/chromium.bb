// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include "chrome/test/ui/ui_test.h"

class ChromeProcessUtilTest : public UITest {
};

// Flaky on Mac only. See http://crbug.com/79175
TEST_F(ChromeProcessUtilTest, FLAKY_SanityTest) {
  ChromeProcessList processes = GetRunningChromeProcesses(browser_process_id());
  EXPECT_FALSE(processes.empty());
  QuitBrowser();
  EXPECT_EQ(-1, browser_process_id());
  processes = GetRunningChromeProcesses(browser_process_id());
  EXPECT_TRUE(processes.empty());
}
