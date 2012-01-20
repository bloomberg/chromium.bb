// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"

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
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTest, ToastCrasher) {}
