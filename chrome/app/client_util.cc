// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/app/image_pre_reader_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Expects that |dir| has a trailing backslash. |dir| is modified so it
// contains the full path that was tried. Caller must check for the return
// value not being null to determine if this path contains a valid dll.
HMODULE LoadChromeWithDirectory(string16* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
#if !defined(CHROME_MULTIPLE_DLL)
  const wchar_t* dll_name = installer::kChromeDll;
#else
  const wchar_t* dll_name = cmd_line.HasSwitch(switches::kProcessType) ?
      installer::kChromeChildDll : installer::kChromeDll;
#endif
  dir->append(dll_name);

#if !defined(WIN_DISABLE_PREREAD)
  // On Win7 with Syzygy, pre-read is a win. There've very little difference
  // between 25% and 100%. For cold starts, with or without prefetch 25%
  // performs slightly better than 100%. On XP, pre-read is generally a
  // performance loss.
  if (!cmd_line.HasSwitch(switches::kProcessType)) {
    const size_t kStepSize = 1024 * 1024;
    uint8 percent = base::win::GetVersion() > base::win::VERSION_XP ? 25 : 0;
    ImagePreReader::PartialPreReadImage(dir->c_str(), percent, kStepSize);
  }
#endif

  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

void RecordDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(true, system_level);
}

void ClearDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(false, system_level);
}

}  // namespace

string16 GetExecutablePath() {
  wchar_t path[MAX_PATH];
  ::GetModuleFileNameW(NULL, path, MAX_PATH);
  if (!::PathRemoveFileSpecW(path))
    return string16();
  string16 exe_path(path);
  return exe_path.append(1, L'\\');
}

string16 GetCurrentModuleVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (file_version_info.get()) {
    string16 version_string(file_version_info->file_version());
    if (Version(WideToASCII(version_string)).IsValid())
      return version_string;
  }
  return string16();
}

//=============================================================================

MainDllLoader::MainDllLoader() : dll_(NULL) {
}

MainDllLoader::~MainDllLoader() {
}

// Loading chrome is an interesting affair. First we try loading from the
// current directory to support run-what-you-compile and other development
// scenarios.
// If that fails then we look at the --chrome-version command line flag to
// determine if we should stick with an older dll version even if a new one is
// available to support upgrade-in-place scenarios.
// If that fails then finally we look at the version resource in the current
// module. This is the expected path for chrome.exe browser instances in an
// installed build.
HMODULE MainDllLoader::Load(string16* out_version, string16* out_file) {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  const string16 dir(GetExecutablePath());
  *out_file = dir;
  HMODULE dll = LoadChromeWithDirectory(out_file);
  if (!dll) {
    // Loading from same directory (for developers) failed.
    string16 version_string;
    if (cmd_line.HasSwitch(switches::kChromeVersion)) {
      // This is used to support Chrome Frame, see http://crbug.com/88589.
      version_string = cmd_line.GetSwitchValueNative(switches::kChromeVersion);

      if (!Version(WideToASCII(version_string)).IsValid()) {
        // If a bogus command line flag was given, then abort.
        LOG(ERROR) << "Invalid command line version: " << version_string;
        return NULL;
      }
    }

    // If no version on the command line, then look at the version resource in
    // the current module and try loading that.
    if (version_string.empty())
      version_string = GetCurrentModuleVersion();

    if (version_string.empty()) {
      LOG(ERROR) << "No valid Chrome version found";
      return NULL;
    }

    *out_file = dir;
    *out_version = version_string;
    out_file->append(*out_version).append(1, L'\\');
    dll = LoadChromeWithDirectory(out_file);
    if (!dll) {
      PLOG(ERROR) << "Failed to load Chrome DLL from " << *out_file;
      return NULL;
    }
  }

  DCHECK(dll);

  return dll;
}

// Launching is a matter of loading the right dll, setting the CHROME_VERSION
// environment variable and just calling the entry point. Derived classes can
// add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          sandbox::SandboxInterfaceInfo* sbox_info) {
  string16 version;
  string16 file;
  dll_ = Load(&version, &file);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(chrome::kChromeVersionEnvVar, WideToUTF8(version));
  // TODO(erikwright): Remove this when http://crbug.com/174953 is fixed and
  // widely deployed.
  env->UnSetVar(env_vars::kGoogleUpdateIsMachineEnvVar);

  InitCrashReporter();
  OnBeforeLaunch(file);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return chrome::RESULT_CODE_BAD_PROCESS_TYPE;

  int rc = entry_point(instance, sbox_info);
  return OnBeforeExit(rc, file);
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
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
 public:
  virtual string16 GetRegistryPath() {
    string16 key(google_update::kRegPathClients);
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    key.append(L"\\").append(dist->GetAppGuid());
    return key;
  }

  virtual void OnBeforeLaunch(const string16& dll_path) {
    RecordDidRun(dll_path);
  }

  virtual int OnBeforeExit(int return_code, const string16& dll_path) {
    // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
    // so we need to reset the did_run signal so omaha does not count
    // this run as active usage.
    if (chrome::RESULT_CODE_NORMAL_EXIT_CANCEL == return_code) {
      ClearDidRun(dll_path);
    }
    return return_code;
  }
};

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 public:
  virtual string16 GetRegistryPath() {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    return dist->GetVersionKey();
  }
};

MainDllLoader* MakeMainDllLoader() {
#if defined(GOOGLE_CHROME_BUILD)
  return new ChromeDllLoader();
#else
  return new ChromiumDllLoader();
#endif
}
