// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/launcher_support/chrome_launcher_support.h"

#include <windows.h>
#include <tchar.h>
#include "base/file_path.h"
#include "base/file_util.h"
#ifndef OFFICIAL_BUILD
#include "base/path_service.h"
#endif
#include "base/string16.h"
#include "base/win/registry.h"

namespace {

// TODO(erikwright): These constants are duplicated all over the place.
// Consolidate them somehow.
const wchar_t kChromeRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

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

}  // namespace

namespace chrome_launcher_support {

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

FilePath GetChromePathForInstallationLevel(InstallationLevel level) {
  using base::win::RegKey;
  HKEY root_key = (level == USER_LEVEL_INSTALLATION ?
                   HKEY_CURRENT_USER :
                   HKEY_LOCAL_MACHINE);
  RegKey reg_key(root_key, kChromeRegClientStateKey, KEY_QUERY_VALUE);

  FilePath chrome_exe_path;

  if (reg_key.Valid()) {
    // Now grab the uninstall string from the appropriate ClientState key
    // and use that as the base for a path to chrome.exe.
    string16 uninstall;
    if (reg_key.ReadValue(kUninstallStringField, &uninstall) == ERROR_SUCCESS) {
      // The uninstall path contains the path to setup.exe which is two levels
      // down from chrome.exe. Move up two levels (plus one to drop the file
      // name) and look for chrome.exe from there.
      FilePath uninstall_path(uninstall);
      chrome_exe_path =
          uninstall_path.DirName().DirName().DirName().Append(kChromeExe);
      if (!file_util::PathExists(chrome_exe_path)) {
        // By way of mild future proofing, look up one to see if there's a
        // chrome.exe in the version directory
        chrome_exe_path =
            uninstall_path.DirName().DirName().Append(kChromeExe);
      }
    }
  }

  if (file_util::PathExists(chrome_exe_path))
    return chrome_exe_path;

  return FilePath();
}

}  // namespace chrome_launcher_support
