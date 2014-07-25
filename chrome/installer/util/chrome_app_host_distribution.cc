// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome App Host. It overrides the bare minimum of methods necessary to get a
// Chrome App Host installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_app_host_distribution.h"

#include "base/strings/string_util.h"
#include "chrome/common/net/test_server_locations.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/updating_app_registration_data.h"

#include "installer_util_strings.h"  // NOLINT

namespace {

const wchar_t kChromeAppHostGuid[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

}  // namespace

ChromeAppHostDistribution::ChromeAppHostDistribution()
    : BrowserDistribution(
          CHROME_APP_HOST,
          scoped_ptr<AppRegistrationData>(
              new UpdatingAppRegistrationData(kChromeAppHostGuid))) {
}

base::string16 ChromeAppHostDistribution::GetBaseAppName() {
  return L"Google Chrome App Launcher";
}

base::string16 ChromeAppHostDistribution::GetBrowserProgIdPrefix() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromeAppHostDistribution::GetBrowserProgIdDesc() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromeAppHostDistribution::GetDisplayName() {
  return GetShortcutName(SHORTCUT_APP_LAUNCHER);
}

base::string16 ChromeAppHostDistribution::GetShortcutName(
    ShortcutType shortcut_type) {
  DCHECK_EQ(shortcut_type, SHORTCUT_APP_LAUNCHER);
  return installer::GetLocalizedString(IDS_PRODUCT_APP_LAUNCHER_NAME_BASE);
}

base::string16 ChromeAppHostDistribution::GetBaseAppId() {
  // Should be same as AppListController::GetAppModelId().
  return L"ChromeAppList";
}

base::string16 ChromeAppHostDistribution::GetInstallSubDir() {
  return BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES)->GetInstallSubDir();
}

base::string16 ChromeAppHostDistribution::GetPublisherName() {
  const base::string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

base::string16 ChromeAppHostDistribution::GetAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_APP_LAUNCHER_SHORTCUT_TOOLTIP_BASE);
  return app_description;
}

base::string16 ChromeAppHostDistribution::GetLongAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_APP_LAUNCHER_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string ChromeAppHostDistribution::GetSafeBrowsingName() {
  return "googlechromeapphost";
}

std::string ChromeAppHostDistribution::GetNetworkStatsServer() const {
  return chrome_common_net::kEchoTestServerLocation;
}

base::string16 ChromeAppHostDistribution::GetUninstallLinkName() {
  const base::string16& link_name =
      installer::GetLocalizedString(IDS_UNINSTALL_APP_LAUNCHER_BASE);
  return link_name;
}

base::string16 ChromeAppHostDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome App Launcher";
}

BrowserDistribution::DefaultBrowserControlPolicy
    ChromeAppHostDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_UNSUPPORTED;
}

bool ChromeAppHostDistribution::CanCreateDesktopShortcuts() {
  return true;
}

base::string16 ChromeAppHostDistribution::GetIconFilename() {
  return installer::kChromeAppHostExe;
}

bool ChromeAppHostDistribution::GetCommandExecuteImplClsid(
    base::string16* handler_class_uuid) {
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
