// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace profiling {

#if !defined(OS_MACOSX)
class ProfilingBrowserTest : public InProcessBrowserTest {
 protected:
  void RelaunchWithMemlog() {
    // TODO(ajwong): Remove this once Brett lands is process model change so the
    // browser process actually launches the profiling process. Until then, it's
    // okay to have this skip on Mac (which has a different launch setup such
    // that this sort of relaunch doesn't work). See GetCommandLineForRelaunch()
    // function for details.
    // TODO(awong): Can we do this with just SetUpCommandLine() and no Relaunch?
    base::CommandLine new_command_line(GetCommandLineForRelaunch());
    new_command_line.AppendSwitchASCII(switches::kMemlog,
                                       switches::kMemlogModeAll);

    ui_test_utils::BrowserAddedObserver observer;
    base::LaunchProcess(new_command_line, base::LaunchOptionsForTest());

    observer.WaitForSingleNewBrowser();
  }
};

IN_PROC_BROWSER_TEST_F(ProfilingBrowserTest, InterceptsNew) {
  RelaunchWithMemlog();
}
#endif

}  // namespace profiling
