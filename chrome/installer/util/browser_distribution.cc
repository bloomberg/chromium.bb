// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"

#include "installer_util_strings.h"  // NOLINT

using installer::MasterPreferences;

namespace {
// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;
BrowserDistribution* g_chrome_frame_distribution = NULL;

// Returns true if currently running in npchrome_frame.dll
bool IsChromeFrameModule() {
  FilePath module_path;
  PathService::Get(base::FILE_MODULE, &module_path);
  return FilePath::CompareEqualIgnoreCase(module_path.BaseName().value(),
                                          installer::kChromeFrameDll);
}

// Returns true if currently running in ceee_broker.exe
bool IsCeeeBrokerProcess() {
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  return FilePath::CompareEqualIgnoreCase(exe_path.BaseName().value(),
                                          installer::kCeeeBrokerExe);
}

BrowserDistribution::Type GetCurrentDistributionType() {
  static BrowserDistribution::Type type =
      (MasterPreferences::ForCurrentProcess().install_chrome_frame() ||
       IsChromeFrameModule()) ?
          BrowserDistribution::CHROME_FRAME :
          BrowserDistribution::CHROME_BROWSER;
  return type;
}

}  // end namespace

BrowserDistribution::BrowserDistribution(
    const installer::MasterPreferences& prefs)
    : type_(BrowserDistribution::CHROME_BROWSER) {
}

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    const installer::MasterPreferences& prefs,
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass(prefs);
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

BrowserDistribution* BrowserDistribution::GetDistribution() {
  const installer::MasterPreferences& prefs =
      installer::MasterPreferences::ForCurrentProcess();
  return GetSpecificDistribution(GetCurrentDistributionType(), prefs);
}

// static
BrowserDistribution* BrowserDistribution::GetSpecificDistribution(
    BrowserDistribution::Type type,
    const installer::MasterPreferences& prefs) {
  BrowserDistribution* dist = NULL;

  if (type == CHROME_FRAME) {
    dist = GetOrCreateBrowserDistribution<ChromeFrameDistribution>(
        prefs, &g_chrome_frame_distribution);
  } else {
    DCHECK_EQ(CHROME_BROWSER, type);
#if defined(GOOGLE_CHROME_BUILD)
    if (InstallUtil::IsChromeSxSProcess()) {
      dist = GetOrCreateBrowserDistribution<GoogleChromeSxSDistribution>(
          prefs, &g_browser_distribution);
    } else {
      dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
          prefs, &g_browser_distribution);
    }
#else
    dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
        prefs, &g_browser_distribution);
#endif
  }

  return dist;
}

void BrowserDistribution::DoPostUninstallOperations(
    const Version& version, const FilePath& local_data_path,
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
      installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
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

std::wstring BrowserDistribution::GetDistributionData(HKEY root_key) {
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

bool BrowserDistribution::CanSetAsDefault() {
  return true;
}

int BrowserDistribution::GetIconIndex() {
  return 0;
}

bool BrowserDistribution::GetChromeChannel(std::wstring* channel) {
  return false;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    bool incremental_install, bool multi_install,
    installer::InstallStatus install_status) {
}

void BrowserDistribution::LaunchUserExperiment(
    installer::InstallStatus status, const Version& version,
    const installer::Product& installation, bool system_level) {
}


void BrowserDistribution::InactiveUserToastExperiment(int flavor,
    const installer::Product& installation) {
}

std::vector<FilePath> BrowserDistribution::GetKeyFiles() {
  std::vector<FilePath> key_files;
  key_files.push_back(FilePath(installer::kChromeDll));
  return key_files;
}

std::vector<FilePath> BrowserDistribution::GetComDllList() {
  return std::vector<FilePath>();
}

void BrowserDistribution::AppendUninstallCommandLineFlags(
    CommandLine* cmd_line) {
  DCHECK(cmd_line);
  cmd_line->AppendSwitch(installer::switches::kChrome);
}

bool BrowserDistribution::ShouldCreateUninstallEntry() {
  return true;
}

bool BrowserDistribution::SetChannelFlags(
    bool set,
    installer::ChannelInfo* channel_info) {
  return false;
}
