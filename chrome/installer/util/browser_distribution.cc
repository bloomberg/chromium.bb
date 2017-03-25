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

base::string16 BrowserDistribution::GetBaseAppName() {
  return L"Chromium";
}

base::string16 BrowserDistribution::GetDisplayName() {
  return GetShortcutName();
}

base::string16 BrowserDistribution::GetShortcutName() {
  return GetBaseAppName();
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
