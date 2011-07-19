// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

std::wstring ChromiumBinariesDistribution::GetAppGuid() {
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetApplicationName() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetAppShortCutName() {
  return kChromiumBinariesName;
}

std::wstring ChromiumBinariesDistribution::GetAlternateApplicationName() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetBrowserAppId() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetInstallSubDir() {
  return browser_distribution_->GetInstallSubDir();
}

std::wstring ChromiumBinariesDistribution::GetPublisherName() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetAppDescription() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetLongAppDescription() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::string ChromiumBinariesDistribution::GetSafeBrowsingName() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::string();
}

std::wstring ChromiumBinariesDistribution::GetStateKey() {
  return std::wstring(L"Software\\").append(kChromiumBinariesName);
}

std::wstring ChromiumBinariesDistribution::GetStateMediumKey() {
  return std::wstring(L"Software\\").append(kChromiumBinariesName);
}

std::wstring ChromiumBinariesDistribution::GetUninstallLinkName() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetUninstallRegPath() {
  NOTREACHED() << "GetApplicationName unsupported";
  return std::wstring();
}

std::wstring ChromiumBinariesDistribution::GetVersionKey() {
  return std::wstring(L"Software\\").append(kChromiumBinariesName);
}

bool ChromiumBinariesDistribution::CanSetAsDefault() {
  NOTREACHED() << "GetApplicationName unsupported";
  return false;
}

int ChromiumBinariesDistribution::GetIconIndex() {
  NOTREACHED() << "GetApplicationName unsupported";
  return 0;
}

bool ChromiumBinariesDistribution::GetChromeChannel(std::wstring* channel) {
  NOTREACHED() << "GetApplicationName unsupported";
  return false;
}
