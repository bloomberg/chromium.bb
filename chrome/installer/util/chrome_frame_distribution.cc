// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome Frame. It overrides the bare minimum of methods necessary to get a
// Chrome Frame installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_frame_distribution.h"

#include <string>
#include <windows.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"

#include "installer_util_strings.h"

std::wstring ChromeFrameDistribution::GetApplicationName() {
  // TODO(robertshield): localize
  return L"Google Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetAlternateApplicationName() {
  // TODO(robertshield): localize
  return L"Chromium technology in your existing browser";
}

std::wstring ChromeFrameDistribution::GetInstallSubDir() {
  // TODO(robertshield): localize
  return L"Google\\Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer_util::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

std::wstring ChromeFrameDistribution::GetAppDescription() {
  return L"Chrome in a Frame.";
}

std::wstring ChromeFrameDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
  return key;
}

std::wstring ChromeFrameDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
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
  key.append(google_update::kChromeGuid);
  return key;
}

int ChromeFrameDistribution::GetInstallReturnCode(
    installer_util::InstallStatus status) {
  switch (status) {
    case installer_util::FIRST_INSTALL_SUCCESS:
    case installer_util::INSTALL_REPAIRED:
    case installer_util::NEW_VERSION_UPDATED:
    case installer_util::HIGHER_VERSION_EXISTS:
      return 0;  // For Google Update's benefit we need to return 0 for success
    default:
      return status;
  }
}

void ChromeFrameDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer_util::InstallStatus install_status) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  std::wstring ap_key_value;
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(google_update::kChromeGuid);
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
      !key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    LOG(INFO) << "Application key not found.";
  } else {
    const char kMagicSuffix[] = "-full";
    if (LowerCaseEqualsASCII(ap_key_value, kMagicSuffix)) {
      key.DeleteValue(google_update::kRegApField);
    } else {
      size_t pos = ap_key_value.find(ASCIIToWide(kMagicSuffix));
      if (pos != std::wstring::npos) {
        ap_key_value.erase(pos, strlen(kMagicSuffix));
        key.WriteValue(google_update::kRegApField, ap_key_value.c_str());
      }
    }
  }
}
