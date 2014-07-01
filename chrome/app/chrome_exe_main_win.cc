// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/win/windows_version.h"
#include "chrome/app/client_util.h"
#include "chrome/browser/chrome_process_finder_win.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_elf/chrome_elf_main.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/common/result_codes.h"
#include "ui/gfx/win/dpi.h"

namespace {

void CheckSafeModeLaunch() {
  unsigned short k1 = ::GetAsyncKeyState(VK_CONTROL);
  unsigned short k2 = ::GetAsyncKeyState(VK_MENU);
  const unsigned short kPressedMask = 0x8000;
  if ((k1 & kPressedMask) && (k2 & kPressedMask))
    ::SetEnvironmentVariableA(chrome::kSafeModeEnvVar, "1");
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

#if !defined(ADDRESS_SANITIZER)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
#else
// The AddressSanitizer build should be a console program as it prints out stuff
// on stderr.
int main() {
  HINSTANCE instance = GetModuleHandle(NULL);
#endif
  startup_metric_utils::RecordExeMainEntryTime();

  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();

  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  // We don't want to set DPI awareness on Vista because we don't support
  // DirectWrite there. GDI fonts are kerned very badly, so better to leave
  // DPI-unaware and at effective 1.0. See also ShouldUseDirectWrite().
  if (base::win::GetVersion() > base::win::VERSION_VISTA)
    gfx::EnableHighDPISupport();

  if (AttemptFastNotify(*CommandLine::ForCurrentProcess()))
    return 0;

  CheckSafeModeLaunch();

  // Load and launch the chrome dll. *Everything* happens inside.
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;
  return rc;
}
