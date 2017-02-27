// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include <utility>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

using installer::MasterPreferences;

namespace {

const wchar_t kChromiumActiveSetupGuid[] =
    L"{7D2B3E1D-D096-4594-9D8F-A6667F12E0AC}";

const wchar_t kCommandExecuteImplUuid[] =
    L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}";

// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;

}  // namespace

BrowserDistribution::BrowserDistribution()
    : app_reg_data_(base::MakeUnique<NonUpdatingAppRegistrationData>(
          L"Software\\Chromium")) {}

BrowserDistribution::BrowserDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : app_reg_data_(std::move(app_reg_data)) {}

BrowserDistribution::~BrowserDistribution() {}

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass();
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

// static
BrowserDistribution* BrowserDistribution::GetDistribution() {
  BrowserDistribution* dist = NULL;

#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess()) {
    dist = GetOrCreateBrowserDistribution<GoogleChromeSxSDistribution>(
        &g_browser_distribution);
  } else {
    dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
        &g_browser_distribution);
  }
#else
  dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
      &g_browser_distribution);
#endif

  return dist;
}

const AppRegistrationData& BrowserDistribution::GetAppRegistrationData() const {
  return *app_reg_data_;
}

base::string16 BrowserDistribution::GetStateKey() const {
  return app_reg_data_->GetStateKey();
}

base::string16 BrowserDistribution::GetStateMediumKey() const {
  return app_reg_data_->GetStateMediumKey();
}

base::string16 BrowserDistribution::GetVersionKey() const {
  return app_reg_data_->GetVersionKey();
}

void BrowserDistribution::DoPostUninstallOperations(
    const base::Version& version, const base::FilePath& local_data_path,
    const base::string16& distribution_data) {
}

base::string16 BrowserDistribution::GetActiveSetupGuid() {
  return kChromiumActiveSetupGuid;
}

base::string16 BrowserDistribution::GetBaseAppName() {
  return L"Chromium";
}

base::string16 BrowserDistribution::GetDisplayName() {
  return GetShortcutName();
}

base::string16 BrowserDistribution::GetShortcutName() {
  return GetBaseAppName();
}

int BrowserDistribution::GetIconIndex() {
  return icon_resources::kApplicationIndex;
}

base::string16 BrowserDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

base::string16 BrowserDistribution::GetStartMenuShortcutSubfolder(
    Subfolder subfolder_type) {
  switch (subfolder_type) {
    case SUBFOLDER_APPS:
      return installer::GetLocalizedString(IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE);
    default:
      DCHECK_EQ(SUBFOLDER_CHROME, subfolder_type);
      return GetShortcutName();
  }
}

base::string16 BrowserDistribution::GetBaseAppId() {
  return L"Chromium";
}

base::string16 BrowserDistribution::GetBrowserProgIdPrefix() {
  // This used to be "ChromiumHTML", but was forced to become "ChromiumHTM"
  // because of http://crbug.com/153349.  See the declaration of this function
  // in the header file for more details.
  return L"ChromiumHTM";
}

base::string16 BrowserDistribution::GetBrowserProgIdDesc() {
  return L"Chromium HTML Document";
}


base::string16 BrowserDistribution::GetInstallSubDir() {
  return L"Chromium";
}

base::string16 BrowserDistribution::GetPublisherName() {
  return L"Chromium";
}

base::string16 BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

base::string16 BrowserDistribution::GetLongAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "chromium";
}

base::string16 BrowserDistribution::GetDistributionData(HKEY root_key) {
  return L"";
}

base::string16 BrowserDistribution::GetRegistryPath() {
  return base::string16(L"Software\\").append(GetInstallSubDir());
}

base::string16 BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
}

BrowserDistribution::DefaultBrowserControlPolicy
    BrowserDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_FULL_CONTROL;
}

bool BrowserDistribution::CanCreateDesktopShortcuts() {
  return true;
}

base::string16 BrowserDistribution::GetCommandExecuteImplClsid() {
  return kCommandExecuteImplUuid;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}

bool BrowserDistribution::ShouldSetExperimentLabels() {
  return false;
}

bool BrowserDistribution::HasUserExperiments() {
  return false;
}
