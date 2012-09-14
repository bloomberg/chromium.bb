// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/launcher_support/chrome_launcher_support.h"

#include <windows.h>
#include <tchar.h>
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#ifndef OFFICIAL_BUILD
#include "base/path_service.h"
#endif
#include "base/string16.h"
#include "base/win/registry.h"

namespace chrome_launcher_support {

namespace {

// TODO(huangs) Refactor the constants: http://crbug.com/148538
const wchar_t kGoogleRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\";

// Copied from chrome_appid.cc.
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Copied from google_chrome_distribution.cc.
const wchar_t kBrowserAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Copied from util_constants.cc.
const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kChromeExe[] = L"chrome.exe";

#ifndef OFFICIAL_BUILD
FilePath GetDevelopmentChrome() {
  FilePath current_directory;
  if (PathService::Get(base::DIR_EXE, &current_directory)) {
    FilePath chrome_exe_path(current_directory.Append(kChromeExe));
    if (file_util::PathExists(chrome_exe_path))
      return chrome_exe_path;
  }
  return FilePath();
}
#endif

// Reads the path to setup.exe from the value "UninstallString" within the
// specified product's "ClientState" registry key. Returns an empty FilePath if
// an error occurs or the product is not installed at the specified level.
FilePath GetSetupExeFromRegistry(InstallationLevel level,
                                 const wchar_t* app_guid) {
  HKEY root_key = (level == USER_LEVEL_INSTALLATION) ?
      HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  string16 subkey(kGoogleRegClientStateKey);
  subkey.append(app_guid);
  base::win::RegKey reg_key;
  if (reg_key.Open(root_key, subkey.c_str(),
                   KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    string16 uninstall;
    if (reg_key.ReadValue(kUninstallStringField, &uninstall) == ERROR_SUCCESS) {
      FilePath setup_exe_path(uninstall);
      if (file_util::PathExists(setup_exe_path))
        return setup_exe_path;
    }
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
  FilePath setup_exe_path(GetSetupExeForInstallationLevel(level));
  if (!setup_exe_path.empty()) {
    // The uninstall path contains the path to setup.exe which is two levels
    // down from chrome.exe. Move up two levels (plus one to drop the file
    // name) and look for chrome.exe from there.
    FilePath chrome_exe_path(
        setup_exe_path.DirName().DirName().DirName().Append(kChromeExe));
    if (!file_util::PathExists(chrome_exe_path)) {
      // By way of mild future proofing, look up one to see if there's a
      // chrome.exe in the version directory
      chrome_exe_path = chrome_exe_path.DirName().DirName().Append(kChromeExe);
    }
    if (file_util::PathExists(chrome_exe_path))
      return chrome_exe_path;
  }
  return FilePath();
}

FilePath GetAnyChromePath() {
  FilePath chrome_path;
#ifndef OFFICIAL_BUILD
  // For development mode, chrome.exe should be in same dir as the stub.
  chrome_path = GetDevelopmentChrome();
#endif
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(SYSTEM_LEVEL_INSTALLATION);
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(USER_LEVEL_INSTALLATION);
  return chrome_path;
}

}  // namespace chrome_launcher_support
