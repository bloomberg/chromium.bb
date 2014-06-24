// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/app/content_main.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(USE_AURA)
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/base/test/ui_controls_aura.h"
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/test/ui_controls_factory_desktop_aurax11.h"
#endif
#endif

#if defined(OS_CHROMEOS)
#include "ash/test/ui_controls_factory_ash.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "chrome/app/chrome_breakpad_client.h"
#endif

namespace {

class ChromeTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  explicit ChromeTestLauncherDelegate(ChromeTestSuiteRunner* runner)
      : runner_(runner) {}
  virtual ~ChromeTestLauncherDelegate() {}

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return runner_->RunTestSuite(argc, argv);
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line, const base::FilePath& temp_data_dir) OVERRIDE {
    CommandLine new_command_line(command_line->GetProgram());
    CommandLine::SwitchMap switches = command_line->GetSwitches();

    // Strip out user-data-dir if present.  We will add it back in again later.
    switches.erase(switches::kUserDataDir);

    for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
         iter != switches.end(); ++iter) {
      new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
    }

    new_command_line.AppendSwitchPath(switches::kUserDataDir, temp_data_dir);

    // file:// access for ChromeOS.
    new_command_line.AppendSwitch(switches::kAllowFileAccess);

    *command_line = new_command_line;
    return true;
  }

 protected:
  virtual content::ContentMainDelegate* CreateContentMainDelegate() OVERRIDE {
    return new ChromeMainDelegate();
  }

  virtual void AdjustDefaultParallelJobs(int* default_jobs) OVERRIDE {
#if defined(OS_WIN)
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAshBrowserTests)) {
      *default_jobs = 1;
      fprintf(stdout,
              "Disabling test parallelization for --ash-browsertests.\n");
      fflush(stdout);
    }
#endif  // defined(OS_WIN)
  }

 private:
  ChromeTestSuiteRunner* runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTestLauncherDelegate);
};

}  // namespace

int LaunchChromeTests(int default_jobs,
                      ChromeTestSuiteRunner* runner,
                      int argc,
                      char** argv) {
#if defined(OS_MACOSX)
  chrome_browser_application_mac::RegisterBrowserCrApp();
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // We leak this pointer intentionally. The breakpad client needs to outlive
  // all other code.
  chrome::ChromeBreakpadClient* breakpad_client =
      new chrome::ChromeBreakpadClient();
  ANNOTATE_LEAKING_OBJECT_PTR(breakpad_client);
  breakpad::SetBreakpadClient(breakpad_client);
#endif

  ChromeTestLauncherDelegate launcher_delegate(runner);
  return content::LaunchTests(&launcher_delegate, default_jobs, argc, argv);
}
