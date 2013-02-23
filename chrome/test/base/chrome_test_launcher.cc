// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <stack>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/process_util.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_VIEWS)
#include "ui/views/focus/accelerator_handler.h"
#endif

#if defined(USE_AURA)
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_controls_aura.h"
#endif

const char kEmptyTestName[] = "InProcessBrowserTest.Empty";

class ChromeTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  ChromeTestLauncherDelegate() {}
  virtual ~ChromeTestLauncherDelegate() {}

  virtual std::string GetEmptyTestName() OVERRIDE {
    return kEmptyTestName;
  }

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return ChromeTestSuite(argc, argv).Run();
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

  virtual void PreRunMessageLoop(base::RunLoop* run_loop) OVERRIDE {
#if !defined(USE_AURA) && defined(TOOLKIT_VIEWS)
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      linked_ptr<views::AcceleratorHandler> handler(
          new views::AcceleratorHandler);
      handlers_.push(handler);
      run_loop->set_dispatcher(handler.get());
    }
#endif
  }

  virtual void PostRunMessageLoop() OVERRIDE {
#if !defined(USE_AURA) && defined(TOOLKIT_VIEWS)
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      DCHECK_EQ(handlers_.empty(), false);
      handlers_.pop();
    }
#endif
  }

 protected:
  virtual content::ContentMainDelegate* CreateContentMainDelegate() OVERRIDE {
#if defined(OS_WIN) || defined (OS_LINUX)
    return new ChromeMainDelegate();
#else
    // This delegate is only guaranteed to link on linux and windows, so just
    // bail out if we are on any other platform.
    NOTREACHED();
    return NULL;
#endif
  }

 private:
#if !defined(USE_AURA) && defined(TOOLKIT_VIEWS)
  std::stack<linked_ptr<views::AcceleratorHandler> > handlers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeTestLauncherDelegate);
};

int main(int argc, char** argv) {
// http://crbug.com/163931 Disabled until interactive_ui_tests ready on Linux
// Aura.
#if defined(OS_LINUX) && defined(USE_AURA) && !defined(OS_CHROMEOS)
  base::FilePath bin_dir;
  CHECK(file_util::ReadSymbolicLink(
      base::FilePath(base::kProcSelfExe), &bin_dir));
  std::string filename = bin_dir.value();
  // http://crbug.com/154081: early exit until interactive_ui_tests are green.
  if (EndsWith(filename, "interactive_ui_tests", false)) {
    LOG(INFO) << "interactive_ui_tests on Linux Aura are not ready yet.";
    return 0;
  }
#endif

#if defined(OS_MACOSX)
  chrome_browser_application_mac::RegisterBrowserCrApp();
#endif

// Only allow ui_controls to be used in interactive_ui_tests, since they depend
// on focus and can't be sharded.
#if defined(INTERACTIVE_TESTS)

#if defined(OS_CHROMEOS)
  ui_controls::InstallUIControlsAura(ui_controls::CreateAshUIControls());
#elif defined(USE_AURA)
  // TODO(win_ash): when running interactive_ui_tests for Win Ash, use above.
  ui_controls::InstallUIControlsAura(ui_controls::CreateUIControlsAura(NULL));
#endif

#endif

  ChromeTestLauncherDelegate launcher_delegate;
  return content::LaunchTests(&launcher_delegate, argc, argv);
}
