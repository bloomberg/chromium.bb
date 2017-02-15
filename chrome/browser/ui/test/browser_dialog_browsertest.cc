// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor_switches.h"

namespace {

// Switch for BrowserDialogTest.Invoke to spawn a subprocess testing the
// provided argument under a consistent setup.
constexpr const char kDialogSwitch[] = "dialog";

// Pattern to search in test names that indicate support for dialog testing.
constexpr const char kDialogPattern[] = "InvokeDialog_";

}  // namespace

// Adds a browser_test entry point into the dialog testing framework. Without a
// --dialog specified, just lists the available dialogs and exits.
TEST(BrowserDialogTest, Invoke) {
  const base::CommandLine& invoker = *base::CommandLine::ForCurrentProcess();
  const std::string dialog_name = invoker.GetSwitchValueASCII(kDialogSwitch);

  std::set<std::string> dialog_cases;
  const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const char* name = test_case->GetTestInfo(j)->name();
      if (strstr(name, kDialogPattern))
        dialog_cases.insert(test_case->name() + std::string(".") + name);
    }
  }

  if (dialog_name.empty()) {
    std::string case_list;
    for (const std::string& name : dialog_cases)
      case_list += "\t" + name + "\n";
    VLOG(0) << "\nPass one of the following after --" << kDialogSwitch << "=\n"
            << case_list;
    return;
  }

  auto it = dialog_cases.find(dialog_name);
  ASSERT_NE(it, dialog_cases.end()) << "Dialog '" << dialog_name
                                    << "' not found.";

  base::CommandLine command(invoker);

  // Replace TestBrowserDialog.Invoke with |dialog_name|.
  command.AppendSwitchASCII(base::kGTestFilterFlag, dialog_name);

  base::LaunchOptions options;

  // Disable timeouts and generate screen output if --interactive was specified.
  if (command.HasSwitch(internal::kInteractiveSwitch)) {
    command.AppendSwitchASCII(switches::kUiTestActionMaxTimeout,
                              TestTimeouts::kNoTimeoutSwitchValue);
    command.AppendSwitchASCII(switches::kTestLauncherTimeout,
                              TestTimeouts::kNoTimeoutSwitchValue);
    command.AppendSwitch(switches::kEnablePixelOutputInTests);
#if defined(OS_WIN)
    // Under Windows, the child process won't launch without the wait option.
    // See http://crbug.com/688534.
    options.wait = true;
    // Under Windows, dialogs (but not the browser window) created in the
    // spawned browser_test process are invisible for some unknown reason.
    // Pass in --disable-gpu to resolve this for now. See
    // http://crbug.com/687387.
    command.AppendSwitch(switches::kDisableGpu);
#endif
  } else {
    options.wait = true;
  }

  base::LaunchProcess(command, options);
}
