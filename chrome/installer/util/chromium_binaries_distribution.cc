// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#include "chrome/installer/util/google_chrome_binaries_distribution.h"

#include "base/logging.h"

namespace {

const wchar_t kChromiumBinariesName[] = L"Chromium Binaries";

}  // namespace

ChromiumBinariesDistribution::ChromiumBinariesDistribution()
    : BrowserDistribution(CHROME_BINARIES),
      browser_distribution_(
          BrowserDistribution::GetSpecificDistribution(CHROME_BROWSER)) {
}

string16 ChromiumBinariesDistribution::GetAppGuid() {
  return string16();
}

string16 ChromiumBinariesDistribution::GetBaseAppName() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetAppShortCutName() {
  return kChromiumBinariesName;
}

string16 ChromiumBinariesDistribution::GetAlternateApplicationName() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetBrowserAppId() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetInstallSubDir() {
  return browser_distribution_->GetInstallSubDir();
}

string16 ChromiumBinariesDistribution::GetPublisherName() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetAppDescription() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetLongAppDescription() {
  NOTREACHED();
  return string16();
}

std::string ChromiumBinariesDistribution::GetSafeBrowsingName() {
  NOTREACHED();
  return std::string();
}

string16 ChromiumBinariesDistribution::GetStateKey() {
  return string16(L"Software\\").append(kChromiumBinariesName);
}

string16 ChromiumBinariesDistribution::GetStateMediumKey() {
  return string16(L"Software\\").append(kChromiumBinariesName);
}

string16 ChromiumBinariesDistribution::GetUninstallLinkName() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetUninstallRegPath() {
  NOTREACHED();
  return string16();
}

string16 ChromiumBinariesDistribution::GetVersionKey() {
  return string16(L"Software\\").append(kChromiumBinariesName);
}

bool ChromiumBinariesDistribution::CanSetAsDefault() {
  NOTREACHED();
  return false;
}

int ChromiumBinariesDistribution::GetIconIndex() {
  NOTREACHED();
  return 0;
}

bool ChromiumBinariesDistribution::GetChromeChannel(string16* channel) {
  NOTREACHED();
  return false;
}
