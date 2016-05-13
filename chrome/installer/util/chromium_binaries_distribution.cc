// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#include "chrome/installer/util/google_chrome_binaries_distribution.h"

#include <utility>

#include "base/logging.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

namespace {

const wchar_t kChromiumBinariesName[] = L"Chromium Binaries";

}  // namespace

ChromiumBinariesDistribution::ChromiumBinariesDistribution()
    : BrowserDistribution(CHROME_BINARIES,
                          std::unique_ptr<AppRegistrationData>(
                              new NonUpdatingAppRegistrationData(
                                  base::string16(L"Software\\")
                                      .append(kChromiumBinariesName)))),
      browser_distribution_(
          BrowserDistribution::GetSpecificDistribution(CHROME_BROWSER)) {}

ChromiumBinariesDistribution::ChromiumBinariesDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : BrowserDistribution(CHROME_BINARIES, std::move(app_reg_data)),
      browser_distribution_(
          BrowserDistribution::GetSpecificDistribution(CHROME_BROWSER)) {}

base::string16 ChromiumBinariesDistribution::GetBaseAppName() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetBrowserProgIdPrefix() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetBrowserProgIdDesc() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetDisplayName() {
  return kChromiumBinariesName;
}

base::string16 ChromiumBinariesDistribution::GetShortcutName() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetBaseAppId() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetInstallSubDir() {
  return browser_distribution_->GetInstallSubDir();
}

base::string16 ChromiumBinariesDistribution::GetPublisherName() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetAppDescription() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromiumBinariesDistribution::GetLongAppDescription() {
  NOTREACHED();
  return base::string16();
}

std::string ChromiumBinariesDistribution::GetSafeBrowsingName() {
  NOTREACHED();
  return std::string();
}

base::string16 ChromiumBinariesDistribution::GetRegistryPath() {
  NOTREACHED();
  // Handling a NOTREACHED() with anything but a default return value is unusual
  // but in this case returning the empty string would point the caller at the
  // root of the registry which could have disastrous consequences.
  return BrowserDistribution::GetRegistryPath();
}

base::string16 ChromiumBinariesDistribution::GetUninstallRegPath() {
  NOTREACHED();
  return base::string16();
}

BrowserDistribution::DefaultBrowserControlPolicy
    ChromiumBinariesDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_UNSUPPORTED;
}

int ChromiumBinariesDistribution::GetIconIndex() {
  NOTREACHED();
  return 0;
}

bool ChromiumBinariesDistribution::GetChromeChannel(base::string16* channel) {
  NOTREACHED();
  return false;
}

base::string16 ChromiumBinariesDistribution::GetCommandExecuteImplClsid() {
  return base::string16();
}
