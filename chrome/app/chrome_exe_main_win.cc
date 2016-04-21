// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <malloc.h>
#include <shellscalingapi.h>
#include <stddef.h>
#include <tchar.h>

#include <algorithm>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome/app/main_dll_loader_win.h"
#include "chrome/browser/chrome_process_finder_win.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome_elf/chrome_elf_main.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/content/app/crash_switches.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/content/app/run_as_crashpad_handler_win.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/pre_read_field_trial_utils_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

namespace {

base::LazyInstance<ChromeCrashReporterClient>::Leaky g_chrome_crash_client =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::vector<crash_reporter::UploadedReport>>::Leaky
    g_uploaded_reports = LAZY_INSTANCE_INITIALIZER;

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

// Returns true if |command_line| contains a /prefetch:# argument where # is in
// [1, 8].
bool HasValidWindowsPrefetchArgument(const base::CommandLine& command_line) {
  const base::char16 kPrefetchArgumentPrefix[] = L"/prefetch:";

  for (const auto& arg : command_line.argv()) {
    if (arg.size() == arraysize(kPrefetchArgumentPrefix) &&
        base::StartsWith(arg, kPrefetchArgumentPrefix,
                         base::CompareCase::SENSITIVE)) {
      return arg[arraysize(kPrefetchArgumentPrefix) - 1] >= L'1' &&
             arg[arraysize(kPrefetchArgumentPrefix) - 1] <= L'8';
    }
  }
  return false;
}

// Some users are getting stuck in compatibility mode. Try to help them escape.
// See http://crbug.com/581499. Returns true if a compatibility mode entry was
// removed.
bool RemoveAppCompatFlagsEntry() {
  base::FilePath current_exe;
  if (!PathService::Get(base::FILE_EXE, &current_exe))
    return false;
  if (!current_exe.IsAbsolute())
    return false;
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows "
               L"NT\\CurrentVersion\\AppCompatFlags\\Layers",
               KEY_READ | KEY_WRITE) == ERROR_SUCCESS) {
    std::wstring layers;
    if (key.ReadValue(current_exe.value().c_str(), &layers) == ERROR_SUCCESS) {
      std::vector<base::string16> tokens = base::SplitString(
          layers, L" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      size_t initial_size = tokens.size();
      static const wchar_t* const kCompatModeTokens[] = {
          L"WIN95",       L"WIN98",       L"WIN4SP5",  L"WIN2000",  L"WINXPSP2",
          L"WINXPSP3",    L"VISTARTM",    L"VISTASP1", L"VISTASP2", L"WIN7RTM",
          L"WINSRV03SP1", L"WINSRV08SP1", L"WIN8RTM",
      };
      for (const wchar_t* compat_mode_token : kCompatModeTokens) {
        tokens.erase(
            std::remove(tokens.begin(), tokens.end(), compat_mode_token),
            tokens.end());
      }
      LONG result;
      if (tokens.empty()) {
        result = key.DeleteValue(current_exe.value().c_str());
      } else {
        base::string16 without_compat_mode_tokens =
            base::JoinString(tokens, L" ");
        result = key.WriteValue(current_exe.value().c_str(),
                                without_compat_mode_tokens.c_str());
      }

      // Return if we changed anything so that we can restart.
      return tokens.size() != initial_size && result == ERROR_SUCCESS;
    }
  }
  return false;
}

}  // namespace

// This helper is looked up in the browser to retrieve the crash reports. See
// CrashUploadListCrashpad. Note that we do not pass an std::vector here,
// because we do not want to allocate/free in different modules. The returned
// pointer is read-only.
extern "C" __declspec(dllexport) void GetUploadedReportsImpl(
    const crash_reporter::UploadedReport** reports,
    size_t* report_count) {
  crash_reporter::GetUploadedReports(g_uploaded_reports.Pointer());
  *reports = g_uploaded_reports.Pointer()->data();
  *report_count = g_uploaded_reports.Pointer()->size();
}

#if !defined(WIN_CONSOLE_APP)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
#else
int main() {
  HINSTANCE instance = GetModuleHandle(nullptr);
#endif
  // Initialize the CommandLine singleton from the environment.
  base::CommandLine::Init(0, nullptr);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  startup_metric_utils::InitializePreReadOptions(
      BrowserDistribution::GetDistribution()->GetRegistryPath());

  // Confirm that an explicit prefetch profile is used for all process types
  // except for the browser process. Any new process type will have to assign
  // itself a prefetch id. See kPrefetchArgument* constants in
  // content_switches.cc for details.
  DCHECK(!startup_metric_utils::GetPreReadOptions().use_prefetch_argument ||
         process_type.empty() ||
         HasValidWindowsPrefetchArgument(*command_line));

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess());
  }

  crash_reporter::SetCrashReporterClient(g_chrome_crash_client.Pointer());
  crash_reporter::InitializeCrashpadWithEmbeddedHandler(process_type.empty(),
                                                        process_type);

  startup_metric_utils::RecordExeMainEntryPointTime(base::Time::Now());

  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  // We don't want to set DPI awareness on pre-Win7 because we don't support
  // DirectWrite there. GDI fonts are kerned very badly, so better to leave
  // DPI-unaware and at effective 1.0. See also ShouldUseDirectWrite().
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    EnableHighDPISupport();

  if (AttemptFastNotify(*command_line))
    return 0;

  RemoveAppCompatFlagsEntry();

  // Load and launch the chrome dll. *Everything* happens inside.
  VLOG(1) << "About to load main DLL.";
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;
  return rc;
}
