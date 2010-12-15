// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome Frame. It overrides the bare minimum of methods necessary to get a
// Chrome Frame installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_frame_distribution.h"

#include <string>

#include "base/string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"

#include "installer_util_strings.h"  // NOLINT

namespace {
const wchar_t kChromeFrameGuid[] = L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
}

ChromeFrameDistribution::ChromeFrameDistribution(
    const installer::MasterPreferences& prefs)
        : BrowserDistribution(prefs) {
  type_ = BrowserDistribution::CHROME_FRAME;
  ceee_ = prefs.install_ceee();
}

std::wstring ChromeFrameDistribution::GetAppGuid() {
  return kChromeFrameGuid;
}

std::wstring ChromeFrameDistribution::GetApplicationName() {
  const std::wstring& product_name =
    installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

std::wstring ChromeFrameDistribution::GetAlternateApplicationName() {
  const std::wstring& product_name =
    installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

std::wstring ChromeFrameDistribution::GetInstallSubDir() {
  return L"Google\\Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

std::wstring ChromeFrameDistribution::GetAppDescription() {
  return L"Chrome in a Frame.";
}

std::wstring ChromeFrameDistribution::GetLongAppDescription() {
  return L"Chrome in a Frame.";
}

std::string ChromeFrameDistribution::GetSafeBrowsingName() {
  return "googlechromeframe";
}

std::wstring ChromeFrameDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

std::wstring ChromeFrameDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

std::wstring ChromeFrameDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::wstring ChromeFrameDistribution::GetUninstallLinkName() {
  return L"Uninstall Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

bool ChromeFrameDistribution::CanSetAsDefault() {
  return false;
}

void ChromeFrameDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer::InstallStatus install_status) {
  GoogleUpdateSettings::UpdateDiffInstallStatus(system_install,
      incremental_install, GetInstallReturnCode(install_status),
      kChromeFrameGuid);
}

std::vector<FilePath> ChromeFrameDistribution::GetKeyFiles() {
  std::vector<FilePath> key_files;
  key_files.push_back(FilePath(installer::kChromeFrameDll));
  if (ceee_) {
    key_files.push_back(FilePath(installer::kCeeeIeDll));
    key_files.push_back(FilePath(installer::kCeeeBrokerExe));
  }
  return key_files;
}

std::vector<FilePath> ChromeFrameDistribution::GetComDllList() {
  std::vector<FilePath> dll_list;
  dll_list.push_back(FilePath(installer::kChromeFrameDll));
  if (ceee_) {
    dll_list.push_back(FilePath(installer::kCeeeInstallHelperDll));
    dll_list.push_back(FilePath(installer::kCeeeIeDll));
  }
  return dll_list;
}

void ChromeFrameDistribution::AppendUninstallCommandLineFlags(
    CommandLine* cmd_line) {
  DCHECK(cmd_line);
  cmd_line->AppendSwitch(installer::switches::kDeleteProfile);
  cmd_line->AppendSwitch(installer::switches::kChromeFrame);
  if (ceee_) {
    cmd_line->AppendSwitch(installer::switches::kCeee);
  }
}
