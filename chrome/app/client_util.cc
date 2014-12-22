// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/app/client_util.h"
#include "chrome/app/image_pre_reader_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/browser_watcher/watcher_client_win.h"
#include "components/browser_watcher/watcher_main_api_win.h"
#include "components/crash/app/breakpad_win.h"
#include "components/crash/app/crash_reporter_client.h"
#include "components/metrics/client_info.h"
#include "content/public/app/startup_helper_win.h"
#include "sandbox/win/src/sandbox.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

base::LazyInstance<chrome::ChromeCrashReporterClient>::Leaky
    g_chrome_crash_client = LAZY_INSTANCE_INITIALIZER;

// Loads |module| after setting the CWD to |module|'s directory. Returns a
// reference to the loaded module on success, or null on error.
HMODULE LoadModuleWithDirectory(const base::FilePath& module, bool pre_read) {
  ::SetCurrentDirectoryW(module.DirName().value().c_str());

  if (pre_read) {
    // We pre-read the binary to warm the memory caches (fewer hard faults to
    // page parts of the binary in).
    const size_t kStepSize = 1024 * 1024;
    size_t percent = 100;
    ImagePreReader::PartialPreReadImage(module.value().c_str(), percent,
                                        kStepSize);
  }

  return ::LoadLibraryExW(module.value().c_str(), nullptr,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

void RecordDidRun(const base::FilePath& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path);
  GoogleUpdateSettings::UpdateDidRunState(true, system_level);
}

void ClearDidRun(const base::FilePath& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path);
  GoogleUpdateSettings::UpdateDidRunState(false, system_level);
}

bool InMetroMode() {
  return (wcsstr(
      ::GetCommandLineW(), L" -ServerName:DefaultBrowserServer") != nullptr);
}

typedef int (*InitMetro)();

// Returns the directory in which the currently running executable resides.
base::FilePath GetExecutableDir() {
  base::char16 path[MAX_PATH];
  ::GetModuleFileNameW(nullptr, path, MAX_PATH);
  return base::FilePath(path).DirName();
}

}  // namespace

base::string16 GetCurrentModuleVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (file_version_info.get()) {
    base::string16 version_string(file_version_info->file_version());
    if (Version(base::UTF16ToASCII(version_string)).IsValid())
      return version_string;
  }
  return base::string16();
}

//=============================================================================

MainDllLoader::MainDllLoader()
  : dll_(nullptr), metro_mode_(InMetroMode()) {
}

MainDllLoader::~MainDllLoader() {
}

// Loading chrome is an interesting affair. First we try loading from the
// current directory to support run-what-you-compile and other development
// scenarios.
// If that fails then we look at the version resource in the current
// module. This is the expected path for chrome.exe browser instances in an
// installed build.
HMODULE MainDllLoader::Load(base::string16* version, base::FilePath* module) {
  const base::char16* dll_name = nullptr;
  if (metro_mode_) {
    dll_name = installer::kChromeMetroDll;
  } else if (process_type_ == "service" || process_type_.empty()) {
    dll_name = installer::kChromeDll;
  } else if (process_type_ == "watcher") {
    dll_name = browser_watcher::kWatcherDll;
  } else {
#if defined(CHROME_MULTIPLE_DLL)
    dll_name = installer::kChromeChildDll;
#else
    dll_name = installer::kChromeDll;
#endif
  }

  const bool pre_read = !metro_mode_;
  base::FilePath module_dir = GetExecutableDir();
  *module = module_dir.Append(dll_name);
  HMODULE dll = LoadModuleWithDirectory(*module, pre_read);
  if (!dll) {
    base::string16 version_string(GetCurrentModuleVersion());
    if (version_string.empty()) {
      LOG(ERROR) << "No valid Chrome version found";
      return nullptr;
    }
    *version = version_string;
    *module = module_dir.Append(version_string).Append(dll_name);
    dll = LoadModuleWithDirectory(*module, pre_read);
    if (!dll) {
      PLOG(ERROR) << "Failed to load Chrome DLL from " << module->value();
      return nullptr;
    }
  }

  DCHECK(dll);
  return dll;
}

// Launching is a matter of loading the right dll, setting the CHROME_VERSION
// environment variable and just calling the entry point. Derived classes can
// add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance) {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  process_type_ = cmd_line.GetSwitchValueASCII(switches::kProcessType);

  base::string16 version;
  base::FilePath file;

  if (metro_mode_) {
    HMODULE metro_dll = Load(&version, &file);
    if (!metro_dll)
      return chrome::RESULT_CODE_MISSING_DATA;

    InitMetro chrome_metro_main =
        reinterpret_cast<InitMetro>(::GetProcAddress(metro_dll, "InitMetro"));
    return chrome_metro_main();
  }

  if (process_type_ == "watcher") {
    // Intentionally leaked.
    HMODULE watcher_dll = Load(&version, &file);
    if (!watcher_dll)
      return chrome::RESULT_CODE_MISSING_DATA;

    browser_watcher::WatcherMainFunction watcher_main =
        reinterpret_cast<browser_watcher::WatcherMainFunction>(
            ::GetProcAddress(watcher_dll,
                             browser_watcher::kWatcherDLLEntrypoint));

    return watcher_main(chrome::kBrowserExitCodesRegistryPath);
  }

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  crash_reporter::SetCrashReporterClient(g_chrome_crash_client.Pointer());
  bool exit_now = true;
  if (process_type_.empty()) {
    if (breakpad::ShowRestartDialogIfCrashed(&exit_now)) {
      // We restarted because of a previous crash. Ask user if we should
      // Relaunch. Only for the browser process. See crbug.com/132119.
      if (exit_now)
        return content::RESULT_CODE_NORMAL_EXIT;
    }
  }
  breakpad::InitCrashReporter(process_type_);

  dll_ = Load(&version, &file);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(chrome::kChromeVersionEnvVar, base::WideToUTF8(version));

  OnBeforeLaunch(process_type_, file);
  DLL_MAIN chrome_main =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  int rc = chrome_main(instance, &sandbox_info);
  return OnBeforeExit(rc, file);
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  if (!dll_)
    return;

  RelaunchChromeBrowserWithNewCommandLineIfNeededFunc relaunch_function =
      reinterpret_cast<RelaunchChromeBrowserWithNewCommandLineIfNeededFunc>(
          ::GetProcAddress(dll_,
                           "RelaunchChromeBrowserWithNewCommandLineIfNeeded"));
  if (!relaunch_function) {
    LOG(ERROR) << "Could not find exported function "
               << "RelaunchChromeBrowserWithNewCommandLineIfNeeded";
  } else {
    relaunch_function();
  }
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 protected:
  void OnBeforeLaunch(const std::string& process_type,
                      const base::FilePath& dll_path) override;
  int OnBeforeExit(int return_code, const base::FilePath& dll_path) override;
};

void ChromeDllLoader::OnBeforeLaunch(const std::string& process_type,
                                     const base::FilePath& dll_path) {
  if (process_type.empty()) {
    RecordDidRun(dll_path);

    // Launch the watcher process if stats collection consent has been granted.
    if (g_chrome_crash_client.Get().GetCollectStatsConsent()) {
      base::char16 exe_path[MAX_PATH];
      ::GetModuleFileNameW(nullptr, exe_path, arraysize(exe_path));

      base::CommandLine cmd_line = base::CommandLine(base::FilePath(exe_path));
      cmd_line.AppendSwitchASCII(switches::kProcessType, "watcher");
      browser_watcher::WatcherClient watcher_client(cmd_line);

      watcher_client.LaunchWatcher();
    }
  }
}

int ChromeDllLoader::OnBeforeExit(int return_code,
                                  const base::FilePath& dll_path) {
  // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
  // so we need to reset the did_run signal so omaha does not count
  // this run as active usage.
  if (chrome::RESULT_CODE_NORMAL_EXIT_CANCEL == return_code) {
    ClearDidRun(dll_path);
  }
  return return_code;
}

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 protected:
  void OnBeforeLaunch(const std::string& process_type,
                      const base::FilePath& dll_path) override {}
  int OnBeforeExit(int return_code, const base::FilePath& dll_path) override {
    return return_code;
  }
};

MainDllLoader* MakeMainDllLoader() {
#if defined(GOOGLE_CHROME_BUILD)
  return new ChromeDllLoader();
#else
  return new ChromiumDllLoader();
#endif
}
