// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/launcher_support/chrome_launcher_support.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"

namespace chrome_launcher_support {

namespace {

// TODO(huangs) Refactor the constants: http://crbug.com/148538
const wchar_t kGoogleRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState";

// Copied from chrome_appid.cc.
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Copied from google_chrome_distribution.cc.
const wchar_t kBrowserAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Copied from util_constants.cc.
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kUninstallStringField[] = L"UninstallString";

// Reads a string value from the specified product's "ClientState" registry key.
// Returns true iff the value is present and successfully read.
bool GetClientStateValue(InstallationLevel level,
                         const wchar_t* app_guid,
                         const wchar_t* value_name,
                         base::string16* value) {
  HKEY root_key = (level == USER_LEVEL_INSTALLATION) ?
      HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  base::string16 subkey(kGoogleRegClientStateKey);
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

// Reads the path to setup.exe from the value "UninstallString" within the
// specified product's "ClientState" registry key. Returns an empty FilePath if
// an error occurs or the product is not installed at the specified level.
base::FilePath GetSetupExeFromRegistry(InstallationLevel level,
                                       const wchar_t* app_guid) {
  base::string16 uninstall;
  if (GetClientStateValue(level, app_guid, kUninstallStringField, &uninstall)) {
    base::FilePath setup_exe_path(uninstall);
    if (base::PathExists(setup_exe_path))
      return setup_exe_path;
  }
  return base::FilePath();
}

// Returns the path to an existing setup.exe at the specified level, if it can
// be found via Omaha client state.
base::FilePath GetSetupExeForInstallationLevel(InstallationLevel level) {
  // Look in the registry for Chrome Binaries first.
  base::FilePath setup_exe_path(
      GetSetupExeFromRegistry(level, kBinariesAppGuid));
  // If the above fails, look in the registry for Chrome next.
  if (setup_exe_path.empty())
    setup_exe_path = GetSetupExeFromRegistry(level, kBrowserAppGuid);
  // If we fail again, then setup_exe_path would be empty.
  return setup_exe_path;
}

// Returns the path to an installed |exe_file| (e.g. chrome.exe) at the
// specified level, given |setup_exe_path| from Omaha client state.  Returns
// empty base::FilePath if none found, or if |setup_exe_path| is empty.
base::FilePath FindExeRelativeToSetupExe(const base::FilePath setup_exe_path,
                                         const wchar_t* exe_file) {
  if (!setup_exe_path.empty()) {
    // The uninstall path contains the path to setup.exe, which is two levels
    // down from |exe_file|. Move up two levels (plus one to drop the file
    // name) and look for chrome.exe from there.
    base::FilePath exe_path(
        setup_exe_path.DirName().DirName().DirName().Append(exe_file));
    if (base::PathExists(exe_path))
      return exe_path;
    // By way of mild future proofing, look up one to see if there's a
    // |exe_file| in the version directory
    exe_path = setup_exe_path.DirName().DirName().Append(exe_file);
    if (base::PathExists(exe_path))
      return exe_path;
  }
  return base::FilePath();
}

}  // namespace

base::FilePath GetChromePathForInstallationLevel(InstallationLevel level) {
  return FindExeRelativeToSetupExe(
      GetSetupExeForInstallationLevel(level), kChromeExe);
}

base::FilePath GetAnyChromePath() {
  base::FilePath chrome_path;
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(SYSTEM_LEVEL_INSTALLATION);
  if (chrome_path.empty())
    chrome_path = GetChromePathForInstallationLevel(USER_LEVEL_INSTALLATION);
  return chrome_path;
}

}  // namespace chrome_launcher_support
