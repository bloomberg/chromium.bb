// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/lock.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"

#include "installer_util_strings.h"

namespace {
// Returns true if currently running in npchrome_frame.dll
bool IsChromeFrameModule() {
  FilePath module_path;
  PathService::Get(base::FILE_MODULE, &module_path);
  return FilePath::CompareEqualIgnoreCase(module_path.BaseName().value(),
                                          installer_util::kChromeFrameDll);
}

// Returns true if currently running in ceee_broker.exe
bool IsCeeeBrokerProcess() {
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  return FilePath::CompareEqualIgnoreCase(exe_path.BaseName().value(),
                                          installer_util::kCeeeBrokerExe);
}

}  // end namespace

BrowserDistribution* BrowserDistribution::GetDistribution() {
  return GetDistribution(InstallUtil::IsChromeFrameProcess() ||
                         IsChromeFrameModule() ||
                         IsCeeeBrokerProcess());
}

BrowserDistribution* BrowserDistribution::GetDistribution(bool chrome_frame) {
  static BrowserDistribution* dist = NULL;
  static Lock dist_lock;
  AutoLock lock(dist_lock);
  if (dist == NULL) {
    if (chrome_frame) {
      // TODO(robertshield): Make one of these for Google Chrome vs
      // non Google Chrome builds?
      dist = new ChromeFrameDistribution();
    } else {
#if defined(GOOGLE_CHROME_BUILD)
      if (InstallUtil::IsChromeSxSProcess()) {
        dist = new GoogleChromeSxSDistribution();
      } else {
        dist = new GoogleChromeDistribution();
      }
#else
      dist = new BrowserDistribution();
#endif
    }
  }
  return dist;
}

void BrowserDistribution::DoPostUninstallOperations(
    const installer::Version& version, const std::wstring& local_data_path,
    const std::wstring& distribution_data) {
}

std::wstring BrowserDistribution::GetAppGuid() {
  return L"";
}

std::wstring BrowserDistribution::GetApplicationName() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetAppShortCutName() {
  return GetApplicationName();
}

std::wstring BrowserDistribution::GetAlternateApplicationName() {
  return L"The Internet";
}

std::wstring BrowserDistribution::GetBrowserAppId() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetInstallSubDir() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetPublisherName() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

std::wstring BrowserDistribution::GetLongAppDescription() {
  const std::wstring& app_description =
      installer_util::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

int BrowserDistribution::GetInstallReturnCode(
    installer_util::InstallStatus status) {
  switch (status) {
    case installer_util::FIRST_INSTALL_SUCCESS:
    case installer_util::INSTALL_REPAIRED:
    case installer_util::NEW_VERSION_UPDATED:
    case installer_util::IN_USE_UPDATED:
      return 0;
    default:
      return status;
  }
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "chromium";
}

std::wstring BrowserDistribution::GetStateKey() {
  return L"Software\\Chromium";
}

std::wstring BrowserDistribution::GetStateMediumKey() {
  return L"Software\\Chromium";
}

std::wstring BrowserDistribution::GetStatsServerURL() {
  return L"";
}

std::wstring BrowserDistribution::GetDistributionData(base::win::RegKey* key) {
  return L"";
}

std::wstring BrowserDistribution::GetUninstallLinkName() {
  return L"Uninstall Chromium";
}

std::wstring BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
}

std::wstring BrowserDistribution::GetVersionKey() {
  return L"Software\\Chromium";
}

std::wstring BrowserDistribution::GetEnvVersionKey() {
  return L"CHROMIUM_VERSION";
}

bool BrowserDistribution::CanSetAsDefault() {
  return true;
}

int BrowserDistribution::GetIconIndex() {
  return 0;
}

bool BrowserDistribution::GetChromeChannel(std::wstring* channel) {
  return false;
}

void BrowserDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer_util::InstallStatus install_status) {
}

void BrowserDistribution::LaunchUserExperiment(
    installer_util::InstallStatus status, const installer::Version& version,
    bool system_install) {
}


void BrowserDistribution::InactiveUserToastExperiment(int flavor,
                                                      bool system_install) {
}
