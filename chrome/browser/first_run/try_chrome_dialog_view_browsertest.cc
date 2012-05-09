// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"

// Unfortunately, this needs to be Windows only for now. Even though this test
// is meant to exercise code that is for Windows only, it is a good general
// canary in the coal mine for problems related to early shutdown (aborted
// startup). Sadly, it times out on platforms other than Windows, so I can't
// enable it for those platforms at the moment. I hope one day our test harness
// will be improved to support this so we can get coverage on other platforms.
// See http://crbug.com/45115 for details.
#if defined(OS_WIN)

// By passing kTryChromeAgain with a magic value > 10000 we cause Chrome
// to exit fairly early.
// Quickly exiting Chrome (regardless of this particular flag -- it
// doesn't do anything other than cause Chrome to quit on startup on
// non-Windows) was a cause of crashes (see bug 34799 for example) so
// this is a useful test of the startup/quick-shutdown cycle.
class TryChromeDialogBrowserTest : public InProcessBrowserTest {
public:
  TryChromeDialogBrowserTest() {}

protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kTryChromeAgain, "10001");
  }
};

// Marking test TryChromeDialogBrowserTest.ToastCrasher DISABLED since its
// breaking the win_rel CQ bot. crbug.com/127396
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTest, DISABLED_ToastCrasher) {}

#endif  // defined(OS_WIN)
