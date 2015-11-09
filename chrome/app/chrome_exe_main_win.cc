// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <malloc.h>
#include <shellscalingapi.h>
#include <tchar.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "chrome/app/main_dll_loader_win.h"
#include "chrome/browser/chrome_process_finder_win.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_elf/chrome_elf_main.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/common/result_codes.h"
#include "ui/gfx/win/dpi.h"

namespace {
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

bool ContainsNonFastStartFlag(const base::CommandLine& command_line) {
  const base::CommandLine::SwitchMap& switches = command_line.GetSwitches();
  if (switches.size() > arraysize(kFastStartSwitches))
    return true;
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (!IsFastStartSwitch(it->first))
      return true;
  }
  return false;
}

bool AttemptFastNotify(const base::CommandLine& command_line) {
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

// Win8.1 supports monitor-specific DPI scaling.
bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef HRESULT(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"),
                         "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
          "Function called twice, or manifest was used.";
    }
  }
  return false;
}

// This function works for Windows Vista through Win8. Win8.1 must use
// SetProcessDpiAwareness[Wrapper].
BOOL SetProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
  SetProcessDPIAwarePtr set_process_dpi_aware_func =
      reinterpret_cast<SetProcessDPIAwarePtr>(
      GetProcAddress(GetModuleHandleA("user32.dll"),
                      "SetProcessDPIAware"));
  return set_process_dpi_aware_func &&
    set_process_dpi_aware_func();
}

void EnableHighDPISupport() {
  if (!SetProcessDpiAwarenessWrapper(PROCESS_SYSTEM_DPI_AWARE)) {
    SetProcessDPIAwareWrapper();
  }
}

void SwitchToLFHeap() {
  // Only needed on XP but harmless on other Windows flavors.
  auto crt_heap = _get_heap_handle();
  ULONG enable_LFH = 2;
  if (HeapSetInformation(reinterpret_cast<HANDLE>(crt_heap),
                         HeapCompatibilityInformation,
                         &enable_LFH, sizeof(enable_LFH))) {
    VLOG(1) << "Low fragmentation heap enabled.";
  }
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
  SwitchToLFHeap();

  startup_metric_utils::RecordExeMainEntryPointTime(base::Time::Now());

  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();

  // Initialize the commandline singleton from the environment.
  base::CommandLine::Init(0, NULL);
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  // We don't want to set DPI awareness on pre-Win7 because we don't support
  // DirectWrite there. GDI fonts are kerned very badly, so better to leave
  // DPI-unaware and at effective 1.0. See also ShouldUseDirectWrite().
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    EnableHighDPISupport();

  if (AttemptFastNotify(*base::CommandLine::ForCurrentProcess()))
    return 0;

  // Load and launch the chrome dll. *Everything* happens inside.
  VLOG(1) << "About to load main DLL.";
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;
  return rc;
}
