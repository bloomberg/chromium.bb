// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_launcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/app/content_main.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "content/common/sandbox_policy.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_types.h"

// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);
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
#if defined(OS_WIN)
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kProcessType)) {
      // TODO(phajdan.jr): Make this not use chrome.dll.
      // This is a child process, call ChromeMain.
      FilePath chrome_path(command_line->GetProgram().DirName());
      chrome_path = chrome_path.Append(chrome::kBrowserResourcesDll);
      HMODULE dll = LoadLibrary(chrome_path.value().c_str());
      DLL_MAIN entry_point =
          reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll, "ChromeMain"));
      if (!entry_point)
        return false;

      // Initialize the sandbox services.
      sandbox::SandboxInterfaceInfo sandbox_info = {0};
      sandbox_info.target_services =
          sandbox::SandboxFactory::GetTargetServices();
      *return_code =
          entry_point(GetModuleHandle(NULL), &sandbox_info, GetCommandLineW());
      return true;
    }
#elif defined(OS_LINUX)
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kProcessType)) {
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

    // Create a new user data dir and pass it to the child.
    ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir() || !temp_dir.IsValid()) {
      LOG(ERROR) << "Error creating temp profile directory";
      return false;
    }
    new_command_line.AppendSwitchPath(switches::kUserDataDir, temp_dir.path());

    // file:// access for ChromeOS.
    new_command_line.AppendSwitch(switches::kAllowFileAccess);

    *command_line = new_command_line;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeTestLauncherDelegate);
};

int main(int argc, char** argv) {
  ChromeTestLauncherDelegate launcher_delegate;
  return test_launcher::LaunchTests(&launcher_delegate, argc, argv);
}
