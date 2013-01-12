// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/launcher_support/chrome_launcher_support.h"

#include <windows.h>
#include <tchar.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/win/registry.h"

#ifndef OFFICIAL_BUILD
#include "base/path_service.h"
#endif

namespace chrome_launcher_support {

namespace {

// TODO(huangs) Refactor the constants: http://crbug.com/148538
const wchar_t kGoogleRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState";

// Copied from binaries_installer_internal.cc
const wchar_t kAppHostAppId[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

// Copied from chrome_appid.cc.
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Copied from google_chrome_distribution.cc.
const wchar_t kBrowserAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Copied from util_constants.cc.
const wchar_t kChromeAppHostExe[] = L"app_host.exe";
const char kChromeAppLauncher[] = "app-launcher";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kUninstallStringField[] = L"UninstallString";

#ifndef OFFICIAL_BUILD
FilePath GetDevelopmentExe(const wchar_t* exe_file) {
  FilePath current_directory;
  if (PathService::Get(base::DIR_EXE, &current_directory)) {
    FilePath chrome_exe_path(current_directory.Append(exe_file));
    if (file_util::PathExists(chrome_exe_path))
      return chrome_exe_path;
  }
  return FilePath();
}
#endif

// Reads a string value from the specified product's "ClientState" registry key.
// Returns true iff the value is present and successfully read.
bool GetClientStateValue(InstallationLevel level,
                         const wchar_t* app_guid,
                         const wchar_t* value_name,
                         string16* value) {
  HKEY root_key = (level == USER_LEVEL_INSTALLATION) ?
      HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  string16 subkey(kGoogleRegClientStateKey);
  subkey.append(1, L'\\').append(app_guid);
  base::win::RegKey reg_key;
  // Google Update always uses 32bit hive.
  if (reg_key.Open(root_key, subkey.c_str(),
                   KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    if (reg_key.ReadValue(value_name, value) == ERROR_SUCCESS) {
      return true;
    }
  }
  return false;
}

bool IsAppLauncherEnabledAtLevel(InstallationLevel level) {
  string16 uninstall_arguments;
  if (GetClientStateValue(level,
                          kAppHostAppId,
                          kUninstallArgumentsField,
                          &uninstall_arguments)) {
    return CommandLine::FromString(L"dummy.exe " + uninstall_arguments)
        .HasSwitch(kChromeAppLauncher) &&
        !GetAppHostPathForInstallationLevel(level).empty();
  }
  return false;
}

// Reads the path to setup.exe from the value "UninstallString" within the
// specified product's "ClientState" registry key. Returns an empty FilePath if
// an error occurs or the product is not installed at the specified level.
FilePath GetSetupExeFromRegistry(InstallationLevel level,
                                 const wchar_t* app_guid) {
  string16 uninstall;
  if (GetClientStateValue(level, app_guid, kUninstallStringField, &uninstall)) {
    FilePath setup_exe_path(uninstall);
    if (file_util::PathExists(setup_exe_path))
      return setup_exe_path;
  }
  return FilePath();
}

// Returns the path to an installed |exe_file| (e.g. chrome.exe, app_host.exe)
// at the specified level, given |setup_exe_path| from Omaha client state.
// Returns empty FilePath if none found, or if |setup_exe_path| is empty.
FilePath FindExeRelativeToSetupExe(const FilePath setup_exe_path,
                                   const wchar_t* exe_file) {
  if (!setup_exe_path.empty()) {
    // The uninstall path contains the path to setup.exe, which is two levels
    // down from |exe_file|. Move up two levels (plus one to drop the file
    // name) and look for chrome.exe from there.
    FilePath exe_path(
        setup_exe_path.DirName().DirName().DirName().Append(exe_file));
    if (file_util::PathExists(exe_path))
      return exe_path;
    // By way of mild future proofing, look up one to see if there's a
    // |exe_file| in the version directory
    exe_path = setup_exe_path.DirName().DirName().Append(exe_file);
    if (file_util::PathExists(exe_path))
      return exe_path;
  }
  return FilePath();
}

}  // namespace

FilePath GetSetupExeForInstallationLevel(InstallationLevel level) {
  // Look in the registry for Chrome Binaries first.
  FilePath setup_exe_path(GetSetupExeFromRegistry(level, kBinariesAppGuid));
  // If the above fails, look in the registry for Chrome next.
  if (setup_exe_path.empty())
    setup_exe_path = GetSetupExeFromRegistry(level, kBrowserAppGuid);
  // If we fail again, then setup_exe_path would be empty.
  return setup_exe_path;
}

FilePath GetChromePathForInstallationLevel(InstallationLevel level) {
  return FindExeRelativeToSetupExe(
      GetSetupExeForInstallationLevel(level), kChromeExe);
}

FilePath GetAppHostPathForInstallationLevel(InstallationLevel level) {
  return FindExeRelativeToSetupExe(
      GetSetupExeFromRegistry(level, kAppHostAppId), kChromeAppHostExe);
}

FilePath GetAnyChromePath() {
  FilePath chrome_path;
#ifndef OFFICIAL_BUILD
  // For development mode, chrome.exe should be in same dir as the stub.
  chrome_path = GetDevelopmentExe(kChromeExe);
#endif
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(SYSTEM_LEVEL_INSTALLATION);
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(USER_LEVEL_INSTALLATION);
  return chrome_path;
}

FilePath GetAnyAppHostPath() {
  FilePath app_host_path;
#ifndef OFFICIAL_BUILD
  // For development mode, app_host.exe should be in same dir as chrome.exe.
  app_host_path = GetDevelopmentExe(kChromeAppHostExe);
#endif
  if (app_host_path.empty()) {
    app_host_path = GetAppHostPathForInstallationLevel(
        SYSTEM_LEVEL_INSTALLATION);
  }
  if (app_host_path.empty())
    app_host_path = GetAppHostPathForInstallationLevel(USER_LEVEL_INSTALLATION);
  return app_host_path;
}

bool IsAppHostPresent() {
  FilePath app_host_exe = GetAnyAppHostPath();
  return !app_host_exe.empty();
}

bool IsAppLauncherPresent() {
  return IsAppLauncherEnabledAtLevel(USER_LEVEL_INSTALLATION) ||
      IsAppLauncherEnabledAtLevel(SYSTEM_LEVEL_INSTALLATION);
}

}  // namespace chrome_launcher_support
