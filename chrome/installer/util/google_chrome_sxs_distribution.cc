// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines implementation of GoogleChromeSxSDistribution.

#include "chrome/installer/util/google_chrome_sxs_distribution.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/updating_app_registration_data.h"

namespace {

const wchar_t kChromeSxSGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
const wchar_t kBrowserAppId[] = L"ChromeCanary";
const wchar_t kBrowserProgIdPrefix[] = L"ChromeSSHTM";
const wchar_t kBrowserProgIdDesc[] = L"Chrome Canary HTML Document";
const wchar_t kCommandExecuteImplUuid[] =
    L"{1BEAC3E3-B852-44F4-B468-8906C062422E}";

}  // namespace

GoogleChromeSxSDistribution::GoogleChromeSxSDistribution()
    : GoogleChromeDistribution(std::unique_ptr<AppRegistrationData>(
          new UpdatingAppRegistrationData(kChromeSxSGuid))) {}

base::string16 GoogleChromeSxSDistribution::GetBaseAppName() {
  return L"Google Chrome Canary";
}

base::string16 GoogleChromeSxSDistribution::GetShortcutName() {
  return installer::GetLocalizedString(IDS_SXS_SHORTCUT_NAME_BASE);
}

base::string16 GoogleChromeSxSDistribution::GetStartMenuShortcutSubfolder(
    Subfolder subfolder_type) {
  switch (subfolder_type) {
    case SUBFOLDER_APPS:
      return installer::GetLocalizedString(
          IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY_BASE);
    default:
      DCHECK_EQ(subfolder_type, SUBFOLDER_CHROME);
      return GetShortcutName();
  }
}

base::string16 GoogleChromeSxSDistribution::GetBaseAppId() {
  return kBrowserAppId;
}

base::string16 GoogleChromeSxSDistribution::GetBrowserProgIdPrefix() {
  return kBrowserProgIdPrefix;
}

base::string16 GoogleChromeSxSDistribution::GetBrowserProgIdDesc() {
  return kBrowserProgIdDesc;
}

base::string16 GoogleChromeSxSDistribution::GetInstallSubDir() {
  return GoogleChromeDistribution::GetInstallSubDir().append(
      installer::kSxSSuffix);
}

base::string16 GoogleChromeSxSDistribution::GetUninstallRegPath() {
  return GoogleChromeDistribution::GetUninstallRegPath().append(
      installer::kSxSSuffix);
}

BrowserDistribution::DefaultBrowserControlPolicy
    GoogleChromeSxSDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_OS_CONTROL_ONLY;
}

int GoogleChromeSxSDistribution::GetIconIndex() {
  return icon_resources::kSxSApplicationIndex;
}

base::string16 GoogleChromeSxSDistribution::GetCommandExecuteImplClsid() {
  return kCommandExecuteImplUuid;
}

bool GoogleChromeSxSDistribution::ShouldSetExperimentLabels() {
  return true;
}

bool GoogleChromeSxSDistribution::HasUserExperiments() {
  return true;
}
