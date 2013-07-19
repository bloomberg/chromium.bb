// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "chrome/app/chrome_breakpad_client.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/app/metro_driver_win.h"
#include "chrome/browser/chrome_process_finder_win.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/breakpad/breakpad_client.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/common/result_codes.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

base::LazyInstance<chrome::ChromeBreakpadClient>::Leaky
    g_chrome_breakpad_client = LAZY_INSTANCE_INITIALIZER;

int RunChrome(HINSTANCE instance) {
  breakpad::SetBreakpadClient(g_chrome_breakpad_client.Pointer());

  bool exit_now = true;
  // We restarted because of a previous crash. Ask user if we should relaunch.
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    if (exit_now)
      return content::RESULT_CODE_NORMAL_EXIT;
  }

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  // Load and launch the chrome dll. *Everything* happens inside.
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance, &sandbox_info);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;
  return rc;
}

// List of switches that it's safe to rendezvous early with. Fast start should
// not be done if a command line contains a switch not in this set.
// Note this is currently stored as a list of two because it's probably faster
// to iterate over this small array than building a map for constant time
// lookups.
const char* const kFastStartSwitches[] = {
  switches::kProfileDirectory,
  switches::kShowAppList,
};

bool IsFastStartSwitch(const std::string& command_line_switch) {
  for (size_t i = 0; i < arraysize(kFastStartSwitches); ++i) {
    if (command_line_switch == kFastStartSwitches[i])
      return true;
  }
  return false;
}

bool ContainsNonFastStartFlag(const CommandLine& command_line) {
  const CommandLine::SwitchMap& switches = command_line.GetSwitches();
  if (switches.size() > arraysize(kFastStartSwitches))
    return true;
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (!IsFastStartSwitch(it->first))
      return true;
  }
  return false;
}

bool AttemptFastNotify(const CommandLine& command_line) {
  if (ContainsNonFastStartFlag(command_line))
    return false;

  base::FilePath user_data_dir;
  if (!chrome::GetDefaultUserDataDirectory(&user_data_dir))
    return false;
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);

  HWND chrome = chrome::FindRunningChromeWindow(user_data_dir);
  if (!chrome)
    return false;
  return chrome::AttemptToNotifyRunningChrome(chrome, true) ==
      chrome::NOTIFY_SUCCESS;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  if (AttemptFastNotify(*CommandLine::ForCurrentProcess()))
    return 0;

  MetroDriver metro_driver;
  if (metro_driver.in_metro_mode())
    return metro_driver.RunInMetro(instance, &RunChrome);
  // Not in metro mode, proceed as normal.
  return RunChrome(instance);
}
