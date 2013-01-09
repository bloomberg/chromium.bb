// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
const wchar_t kChannelName[] = L"canary";
const wchar_t kBrowserAppId[] = L"ChromeCanary";
const int kSxSIconIndex = 4;

}  // namespace

GoogleChromeSxSDistribution::GoogleChromeSxSDistribution()
    : GoogleChromeDistribution() {
  GoogleChromeDistribution::set_product_guid(kChromeSxSGuid);
}

string16 GoogleChromeSxSDistribution::GetBaseAppName() {
  return L"Google Chrome Canary";
}

string16 GoogleChromeSxSDistribution::GetAppShortCutName() {
  const string16& shortcut_name =
      installer::GetLocalizedString(IDS_SXS_SHORTCUT_NAME_BASE);
  return shortcut_name;
}

string16 GoogleChromeSxSDistribution::GetBaseAppId() {
  return kBrowserAppId;
}

string16 GoogleChromeSxSDistribution::GetInstallSubDir() {
  return GoogleChromeDistribution::GetInstallSubDir().append(
      installer::kSxSSuffix);
}

string16 GoogleChromeSxSDistribution::GetUninstallRegPath() {
  return GoogleChromeDistribution::GetUninstallRegPath().append(
      installer::kSxSSuffix);
}

bool GoogleChromeSxSDistribution::CanSetAsDefault() {
  return false;
}

int GoogleChromeSxSDistribution::GetIconIndex() {
  return kSxSIconIndex;
}

bool GoogleChromeSxSDistribution::GetChromeChannel(string16* channel) {
  *channel = kChannelName;
  return true;
}

bool GoogleChromeSxSDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  return false;
}

bool GoogleChromeSxSDistribution::AppHostIsSupported() {
  return false;
}

bool GoogleChromeSxSDistribution::ShouldSetExperimentLabels() {
  return true;
}

string16 GoogleChromeSxSDistribution::ChannelName() {
  return kChannelName;
}
