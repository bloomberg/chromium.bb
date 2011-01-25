// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines implementation of GoogleChromeSxSDistribution.

#include "chrome/installer/util/google_chrome_sxs_distribution.h"

#include "base/command_line.h"
#include "base/logging.h"

#include "installer_util_strings.h"  // NOLINT

namespace {

const wchar_t kChromeSxSGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
const wchar_t kChannelName[] = L"canary build";
const wchar_t kBrowserAppId[] = L"ChromeCanary";
const int kSxSIconIndex = 4;

}  // namespace

GoogleChromeSxSDistribution::GoogleChromeSxSDistribution()
    : GoogleChromeDistribution() {
  GoogleChromeDistribution::set_product_guid(kChromeSxSGuid);
}

std::wstring GoogleChromeSxSDistribution::GetAppShortCutName() {
  const std::wstring& shortcut_name =
      installer::GetLocalizedString(IDS_SXS_SHORTCUT_NAME_BASE);
  return shortcut_name;
}

std::wstring GoogleChromeSxSDistribution::GetBrowserAppId() {
  return kBrowserAppId;
}

std::wstring GoogleChromeSxSDistribution::GetInstallSubDir() {
  return GoogleChromeDistribution::GetInstallSubDir().append(
      installer::kSxSSuffix);
}

std::wstring GoogleChromeSxSDistribution::GetUninstallRegPath() {
  return GoogleChromeDistribution::GetUninstallRegPath().append(
      installer::kSxSSuffix);
}

bool GoogleChromeSxSDistribution::CanSetAsDefault() {
  return false;
}

int GoogleChromeSxSDistribution::GetIconIndex() {
  return kSxSIconIndex;
}

bool GoogleChromeSxSDistribution::GetChromeChannel(std::wstring* channel) {
  *channel = kChannelName;
  return true;
}

std::wstring GoogleChromeSxSDistribution::ChannelName() {
  return kChannelName;
}
