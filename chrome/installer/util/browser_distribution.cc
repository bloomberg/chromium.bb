// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/win/windows_version.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/chromium_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"

#include "installer_util_strings.h"  // NOLINT

using installer::MasterPreferences;

namespace {

const wchar_t kCommandExecuteImplUuid[] =
    L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}";
const wchar_t kDelegateExecuteLibUuid[] =
    L"{7779FB70-B399-454A-AA1A-BAA850032B10}";
const wchar_t kDelegateExecuteLibVersion[] = L"1.0";
const wchar_t kICommandExecuteImplUuid[] =
    L"{0BA0D4E9-2259-4963-B9AE-A839F7CB7544}";

// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;
BrowserDistribution* g_chrome_frame_distribution = NULL;
BrowserDistribution* g_binaries_distribution = NULL;

// Returns true if currently running in npchrome_frame.dll
bool IsChromeFrameModule() {
  FilePath module_path;
  PathService::Get(base::FILE_MODULE, &module_path);
  return FilePath::CompareEqualIgnoreCase(module_path.BaseName().value(),
                                          installer::kChromeFrameDll);
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

// CHROME_BINARIES represents the binaries shared by multi-install products and
// is not a product in and of itself, so it is not present in this collection.
const BrowserDistribution::Type BrowserDistribution::kProductTypes[] = {
  BrowserDistribution::CHROME_BROWSER,
  BrowserDistribution::CHROME_FRAME,
};

const size_t BrowserDistribution::kNumProductTypes =
    arraysize(BrowserDistribution::kProductTypes);

BrowserDistribution::BrowserDistribution()
    : type_(CHROME_BROWSER) {
}

BrowserDistribution::BrowserDistribution(Type type)
    : type_(type) {
}

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

BrowserDistribution* BrowserDistribution::GetDistribution() {
  return GetSpecificDistribution(GetCurrentDistributionType());
}

// static
BrowserDistribution* BrowserDistribution::GetSpecificDistribution(
    BrowserDistribution::Type type) {
  BrowserDistribution* dist = NULL;

  switch (type) {
    case CHROME_BROWSER:
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
      break;

    case CHROME_FRAME:
      dist = GetOrCreateBrowserDistribution<ChromeFrameDistribution>(
          &g_chrome_frame_distribution);
      break;

    default:
      DCHECK_EQ(CHROME_BINARIES, type);
#if defined(GOOGLE_CHROME_BUILD)
      dist = GetOrCreateBrowserDistribution<GoogleChromeBinariesDistribution>(
          &g_binaries_distribution);
#else
      dist = GetOrCreateBrowserDistribution<ChromiumBinariesDistribution>(
          &g_binaries_distribution);
#endif
  }

  return dist;
}

void BrowserDistribution::DoPostUninstallOperations(
    const Version& version, const FilePath& local_data_path,
    const string16& distribution_data) {
}

string16 BrowserDistribution::GetAppGuid() {
  return L"";
}

string16 BrowserDistribution::GetApplicationName() {
  return L"Chromium";
}

string16 BrowserDistribution::GetAppShortCutName() {
  return GetApplicationName();
}

string16 BrowserDistribution::GetAlternateApplicationName() {
  return L"The Internet";
}

string16 BrowserDistribution::GetBrowserAppId() {
  return L"Chromium";
}

string16 BrowserDistribution::GetInstallSubDir() {
  return L"Chromium";
}

string16 BrowserDistribution::GetPublisherName() {
  return L"Chromium";
}

string16 BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

string16 BrowserDistribution::GetLongAppDescription() {
  const string16& app_description =
      installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "chromium";
}

string16 BrowserDistribution::GetStateKey() {
  return L"Software\\Chromium";
}

string16 BrowserDistribution::GetStateMediumKey() {
  return L"Software\\Chromium";
}

string16 BrowserDistribution::GetStatsServerURL() {
  return L"";
}

std::string BrowserDistribution::GetNetworkStatsServer() const {
  return "";
}

std::string BrowserDistribution::GetHttpPipeliningTestServer() const {
  return "";
}

string16 BrowserDistribution::GetDistributionData(HKEY root_key) {
  return L"";
}

string16 BrowserDistribution::GetUninstallLinkName() {
  return L"Uninstall Chromium";
}

string16 BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
}

string16 BrowserDistribution::GetVersionKey() {
  return L"Software\\Chromium";
}

bool BrowserDistribution::CanSetAsDefault() {
  return true;
}

bool BrowserDistribution::CanCreateDesktopShortcuts() {
  return true;
}

int BrowserDistribution::GetIconIndex() {
  return 0;
}

bool BrowserDistribution::GetChromeChannel(string16* channel) {
  return false;
}

bool BrowserDistribution::GetDelegateExecuteHandlerData(
    string16* handler_class_uuid,
    string16* type_lib_uuid,
    string16* type_lib_version,
    string16* interface_uuid) {
  // Chrome's DelegateExecute verb handler is only used for Windows 8 and up.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    if (handler_class_uuid)
      *handler_class_uuid = kCommandExecuteImplUuid;
    if (type_lib_uuid)
      *type_lib_uuid = kDelegateExecuteLibUuid;
    if (type_lib_version)
      *type_lib_version = kDelegateExecuteLibVersion;
    if (interface_uuid)
      *interface_uuid = kICommandExecuteImplUuid;
    return true;
  }
  return false;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}

bool BrowserDistribution::GetExperimentDetails(
    UserExperiment* experiment, int flavor) {
  return false;
}

void BrowserDistribution::LaunchUserExperiment(
    const FilePath& setup_path, installer::InstallStatus status,
    const Version& version, const installer::Product& product,
    bool system_level) {
}


void BrowserDistribution::InactiveUserToastExperiment(int flavor,
    const string16& experiment_group,
    const installer::Product& installation,
    const FilePath& application_path) {
}
