// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome App Host. It overrides the bare minimum of methods necessary to get a
// Chrome App Host installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_app_host_distribution.h"

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

const wchar_t kChromeAppHostGuid[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

}  // namespace

ChromeAppHostDistribution::ChromeAppHostDistribution()
    : BrowserDistribution(CHROME_APP_HOST) {
}

string16 ChromeAppHostDistribution::GetAppGuid() {
  return kChromeAppHostGuid;
}

string16 ChromeAppHostDistribution::GetBaseAppName() {
  return L"Google Chrome App Launcher";
}

string16 ChromeAppHostDistribution::GetAppShortCutName() {
  const string16& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_APP_LAUNCHER_NAME_BASE);
  return product_name;
}

string16 ChromeAppHostDistribution::GetAlternateApplicationName() {
  const string16& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_APP_LAUNCHER_NAME_BASE);
  return product_name;
}

string16 ChromeAppHostDistribution::GetBaseAppId() {
  // Should be same as AppListController::GetAppModelId().
  return L"ChromeAppList";
}

string16 ChromeAppHostDistribution::GetInstallSubDir() {
  return BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES)->GetInstallSubDir();
}

string16 ChromeAppHostDistribution::GetPublisherName() {
  const string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

string16 ChromeAppHostDistribution::GetAppDescription() {
  const string16& app_description =
      installer::GetLocalizedString(IDS_APP_LAUNCHER_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

string16 ChromeAppHostDistribution::GetLongAppDescription() {
  const string16& app_description =
      installer::GetLocalizedString(IDS_APP_LAUNCHER_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string ChromeAppHostDistribution::GetSafeBrowsingName() {
  return "googlechromeapphost";
}

string16 ChromeAppHostDistribution::GetStateKey() {
  string16 key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(kChromeAppHostGuid);
  return key;
}

string16 ChromeAppHostDistribution::GetStateMediumKey() {
  string16 key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(kChromeAppHostGuid);
  return key;
}

string16 ChromeAppHostDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::string ChromeAppHostDistribution::GetNetworkStatsServer() const {
  return chrome_common_net::kEchoTestServerLocation;
}

std::string ChromeAppHostDistribution::GetHttpPipeliningTestServer() const {
  return chrome_common_net::kPipelineTestServerBaseUrl;
}

string16 ChromeAppHostDistribution::GetUninstallLinkName() {
  const string16& link_name =
      installer::GetLocalizedString(IDS_UNINSTALL_APP_LAUNCHER_BASE);
  return link_name;
}

string16 ChromeAppHostDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome App Launcher";
}

string16 ChromeAppHostDistribution::GetVersionKey() {
  string16 key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(kChromeAppHostGuid);
  return key;
}

bool ChromeAppHostDistribution::CanSetAsDefault() {
  return false;
}

bool ChromeAppHostDistribution::CanCreateDesktopShortcuts() {
  return true;
}

string16 ChromeAppHostDistribution::GetIconFilename() {
  return installer::kChromeAppHostExe;
}

bool ChromeAppHostDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  return false;
}

void ChromeAppHostDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
#if defined(GOOGLE_CHROME_BUILD)
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      archive_type, InstallUtil::GetInstallReturnCode(install_status),
      kChromeAppHostGuid);
#endif
}
