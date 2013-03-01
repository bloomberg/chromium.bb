// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_host/update.h"

#include <windows.h>
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"

namespace app_host {

namespace {

// TODO(huangs) Refactor the constants: http://crbug.com/148538
const wchar_t kGoogleRegClientsKey[] = L"Software\\Google\\Update\\Clients\\";

// Copied from util_constants.cc.
const char kMultiInstall[] = "multi-install";
const char kChromeAppLauncher[] = "app-launcher";
const char kVerboseLogging[] = "verbose-logging";

// Copied from google_update_constants.cc.
const wchar_t kRegVersionField[] = L"pv";

// Copied from chrome_appid.cc.
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Copied from google_chrome_distribution.cc.
const wchar_t kBrowserAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Fetches the version of the App Host, directly from the image's version
// resource.
Version GetAppHostVersion() {
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  Version app_host_version(WideToASCII(version_info->product_version()));
  DCHECK(app_host_version.IsValid());
  return app_host_version;
}

// Fetches the app version ("pv" entry) of a Google product from the
// system-level registry, given the app GUID ("{###...###}").
Version GetAppVersionFromRegistry(const wchar_t* app_guid) {
  HKEY root_key = HKEY_LOCAL_MACHINE;
  string16 client_key(kGoogleRegClientsKey);
  client_key.append(app_guid);
  base::win::RegKey reg_key;
  string16 version_str;
  if ((reg_key.Open(root_key, client_key.c_str(),
                    KEY_QUERY_VALUE) == ERROR_SUCCESS) &&
      (reg_key.ReadValue(kRegVersionField, &version_str) == ERROR_SUCCESS)) {
    return Version(WideToASCII(version_str));
  }
  return Version();
}

// Calls setup.exe to update App Host, using the system-level setup.exe.
bool LaunchAppHostUpdate() {
  // Get the path to the setup.exe.
  base::FilePath setup_exe(
      chrome_launcher_support::GetSetupExeForInstallationLevel(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION));
  if (setup_exe.empty()) {
    LOG(ERROR) << "Failed to find setup.exe";
    return false;
  }
  CommandLine cmd_line(setup_exe);
  cmd_line.AppendSwitch(kMultiInstall);
  cmd_line.AppendSwitch(kChromeAppLauncher);
  cmd_line.AppendSwitch(kVerboseLogging);
  LOG(INFO) << "Launching: " << cmd_line.GetCommandLineString();
  return base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

}  // namespace

void EnsureAppHostUpToDate() {
  // Check version from Chrome Binaries first, then Chrome (all system-level).
  Version new_version(GetAppVersionFromRegistry(kBinariesAppGuid));
  if (!new_version.IsValid())
    new_version = GetAppVersionFromRegistry(kBrowserAppGuid);
  if (!new_version.IsValid())
    return;  // Not an error: System-level Chrome might not be installed.
  Version app_host_version(GetAppHostVersion());
  if (app_host_version.CompareTo(new_version) < 0) {
    LOG(INFO) << "Updating App Launcher from " << app_host_version.GetString()
              << " to " << new_version.GetString();
    if (!LaunchAppHostUpdate())
      LOG(ERROR) << "Failed to launch App Launcher update.";
  }
}

}  // namespace app_host
