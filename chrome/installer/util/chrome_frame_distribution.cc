// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome Frame. It overrides the bare minimum of methods necessary to get a
// Chrome Frame installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_frame_distribution.h"

#include "base/string_util.h"
#include "chrome/common/net/test_server_locations.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"

#include "installer_util_strings.h"  // NOLINT

namespace {
const wchar_t kChromeFrameGuid[] = L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
}

ChromeFrameDistribution::ChromeFrameDistribution()
    : BrowserDistribution(CHROME_FRAME) {
}

string16 ChromeFrameDistribution::GetAppGuid() {
  return kChromeFrameGuid;
}

string16 ChromeFrameDistribution::GetBaseAppName() {
  return L"Google Chrome Frame";
}

string16 ChromeFrameDistribution::GetAppShortCutName() {
  const string16& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

string16 ChromeFrameDistribution::GetAlternateApplicationName() {
  const string16& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

string16 ChromeFrameDistribution::GetInstallSubDir() {
  return L"Google\\Chrome Frame";
}

string16 ChromeFrameDistribution::GetPublisherName() {
  const string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

string16 ChromeFrameDistribution::GetAppDescription() {
  return L"Chrome in a Frame.";
}

string16 ChromeFrameDistribution::GetLongAppDescription() {
  return L"Chrome in a Frame.";
}

std::string ChromeFrameDistribution::GetSafeBrowsingName() {
  return "googlechromeframe";
}

string16 ChromeFrameDistribution::GetStateKey() {
  string16 key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

string16 ChromeFrameDistribution::GetStateMediumKey() {
  string16 key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

string16 ChromeFrameDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::string ChromeFrameDistribution::GetNetworkStatsServer() const {
  return chrome_common_net::kEchoTestServerLocation;
}

std::string ChromeFrameDistribution::GetHttpPipeliningTestServer() const {
  return chrome_common_net::kPipelineTestServerBaseUrl;
}

string16 ChromeFrameDistribution::GetUninstallLinkName() {
  return L"Uninstall Chrome Frame";
}

string16 ChromeFrameDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome Frame";
}

string16 ChromeFrameDistribution::GetVersionKey() {
  string16 key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

string16 ChromeFrameDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

int ChromeFrameDistribution::GetIconIndex() {
  return 0;
}

bool ChromeFrameDistribution::CanSetAsDefault() {
  return false;
}

bool ChromeFrameDistribution::CanCreateDesktopShortcuts() {
  return false;
}

bool ChromeFrameDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  return false;
}

void ChromeFrameDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
#if defined(GOOGLE_CHROME_BUILD)
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      archive_type, InstallUtil::GetInstallReturnCode(install_status),
      kChromeFrameGuid);
#endif
}
