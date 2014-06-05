// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_result_codes.h"
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
  TryChromeDialogBrowserTest() {
    set_expected_exit_code(chrome::RESULT_CODE_NORMAL_EXIT_CANCEL);
  }

protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kTryChromeAgain, "10001");
  }
};

// Note to Sheriffs: This test (as you can read about above) simply causes
// Chrome to shutdown early, and, as such, has proven to be pretty good at
// finding problems related to shutdown. Sheriff, before marking this test as
// disabled, please consider that this test is meant to catch when people
// introduce changes that crash Chrome during shutdown and disabling this test
// and moving on removes a safeguard meant to avoid an even bigger thorny mess
// to untangle much later down the line. Disabling the test also means that the
// people who get blamed are not the ones that introduced the crash (in other
// words, don't shoot the messenger). So, please help us avoid additional
// shutdown crashes from creeping in, by doing the following:
// Run chrome.exe --try-chrome-again=10001. This is all that the test does and
// should be enough to trigger the failure. If it is a crash (most likely) then
// look at the callstack and see if any of the components have been touched
// recently. Look at recent changes to startup, such as any change to
// ChromeBrowserMainParts, specifically PreCreateThreadsImpl and see if someone
// has been reordering code blocks for startup. Try reverting any suspicious
// changes to see if it affects the test. History shows that waiting until later
// only makes the problem worse.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTest, ToastCrasher) {}

#endif  // defined(OS_WIN)
