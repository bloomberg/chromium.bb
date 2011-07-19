// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#include "chrome/installer/util/google_chrome_binaries_distribution.h"

#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"

namespace {

const wchar_t kChromeBinariesGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
const wchar_t kChromeBinariesName[] = L"Google Chrome binaries";

}  // namespace

GoogleChromeBinariesDistribution::GoogleChromeBinariesDistribution()
    : ChromiumBinariesDistribution() {
}

std::wstring GoogleChromeBinariesDistribution::GetAppGuid() {
  return kChromeBinariesGuid;
}

std::wstring GoogleChromeBinariesDistribution::GetAppShortCutName() {
  return kChromeBinariesName;
}

std::wstring GoogleChromeBinariesDistribution::GetStateKey() {
  return std::wstring(google_update::kRegPathClientState)
      .append(1, L'\\')
      .append(kChromeBinariesGuid);
}

std::wstring GoogleChromeBinariesDistribution::GetStateMediumKey() {
  return std::wstring(google_update::kRegPathClientStateMedium)
      .append(1, L'\\')
      .append(kChromeBinariesGuid);
}

std::wstring GoogleChromeBinariesDistribution::GetVersionKey() {
  return std::wstring(google_update::kRegPathClients)
      .append(1, L'\\')
      .append(kChromeBinariesGuid);
}

void GoogleChromeBinariesDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      archive_type, InstallUtil::GetInstallReturnCode(install_status),
      kChromeBinariesGuid);
}
