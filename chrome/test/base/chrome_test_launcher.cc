// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/app/content_main.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/src/sandbox_types.h"
#endif  // defined(OS_WIN)

class ChromeTestLauncherDelegate : public test_launcher::TestLauncherDelegate {
 public:
  ChromeTestLauncherDelegate() {
  }

  virtual ~ChromeTestLauncherDelegate() {
  }

  virtual void EarlyInitialize() OVERRIDE {
#if defined(OS_MACOSX)
    chrome_browser_application_mac::RegisterBrowserCrApp();
#endif
  }

  virtual bool Run(int argc, char** argv, int* return_code) OVERRIDE {
#if defined(OS_WIN) || defined(OS_LINUX)
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    bool launch_chrome =
        command_line->HasSwitch(switches::kProcessType) ||
        command_line->HasSwitch(ChromeTestSuite::kLaunchAsBrowser);
#endif
#if defined(OS_WIN)
    if (launch_chrome) {
      sandbox::SandboxInterfaceInfo sandbox_info = {0};
      content::InitializeSandboxInfo(&sandbox_info);
      ChromeMainDelegate chrome_main_delegate;
      *return_code = content::ContentMain(GetModuleHandle(NULL),
                                          &sandbox_info,
                                          &chrome_main_delegate);
      return true;
    }
#elif defined(OS_LINUX)
    if (launch_chrome) {
      ChromeMainDelegate chrome_main_delegate;
      *return_code = content::ContentMain(argc,
                                          const_cast<const char**>(argv),
                                          &chrome_main_delegate);
      return true;
    }
#endif  // defined(OS_WIN)

    return false;
  }

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return ChromeTestSuite(argc, argv).Run();
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line) OVERRIDE {
    CommandLine new_command_line(command_line->GetProgram());
    CommandLine::SwitchMap switches = command_line->GetSwitches();

    // Strip out user-data-dir if present.  We will add it back in again later.
    switches.erase(switches::kUserDataDir);

    for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
         iter != switches.end(); ++iter) {
      new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
    }

    // Clean up previous temp dir.
    // We Take() the directory and delete it ourselves so that the next
    // CreateUniqueTempDir will succeed even if deleting the directory fails.
    if (!temp_dir_.path().empty() &&
        !file_util::DieFileDie(temp_dir_.Take(), true)) {
      LOG(ERROR) << "Error deleting previous temp profile directory";
    }

    // Create a new user data dir and pass it to the child.
    if (!temp_dir_.CreateUniqueTempDir() || !temp_dir_.IsValid()) {
      LOG(ERROR) << "Error creating temp profile directory";
      return false;
    }
    new_command_line.AppendSwitchPath(switches::kUserDataDir, temp_dir_.path());

    // file:// access for ChromeOS.
    new_command_line.AppendSwitch(switches::kAllowFileAccess);

    *command_line = new_command_line;
    return true;
  }

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTestLauncherDelegate);
};

int main(int argc, char** argv) {
  ChromeTestLauncherDelegate launcher_delegate;
  return test_launcher::LaunchTests(&launcher_delegate, argc, argv);
}
