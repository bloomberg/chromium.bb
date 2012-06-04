// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_suite.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_main_delegate.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/src/sandbox_types.h"
#endif  // defined(OS_WIN)

class ContentTestLauncherDelegate : public test_launcher::TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {
  }

  virtual ~ContentTestLauncherDelegate() {
  }

  virtual void EarlyInitialize() OVERRIDE {
  }

  virtual bool Run(int argc, char** argv, int* return_code) OVERRIDE {
#if defined(OS_WIN)
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kProcessType)) {
      sandbox::SandboxInterfaceInfo sandbox_info = {0};
      content::InitializeSandboxInfo(&sandbox_info);
      ShellMainDelegate delegate;
      *return_code =
          content::ContentMain(GetModuleHandle(NULL), &sandbox_info, &delegate);
      return true;
    }
#endif  // defined(OS_WIN)

    return false;
  }

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return base::TestSuite(argc, argv).Run();
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line) OVERRIDE {
    FilePath file_exe;
    if (!PathService::Get(base::FILE_EXE, &file_exe))
      return false;
    command_line->AppendSwitchPath(switches::kBrowserSubprocessPath, file_exe);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

int main(int argc, char** argv) {
  ContentTestLauncherDelegate launcher_delegate;
  return test_launcher::LaunchTests(&launcher_delegate, argc, argv);
}
