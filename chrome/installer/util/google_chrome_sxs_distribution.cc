// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines implementation of GoogleChromeSxSDistribution.

#include "chrome/installer/util/google_chrome_sxs_distribution.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/updating_app_registration_data.h"

namespace {

const wchar_t kChromeSxSGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";

}  // namespace

GoogleChromeSxSDistribution::GoogleChromeSxSDistribution()
    : GoogleChromeDistribution(std::unique_ptr<AppRegistrationData>(
          new UpdatingAppRegistrationData(kChromeSxSGuid))) {}

base::string16 GoogleChromeSxSDistribution::GetBaseAppName() {
  return L"Google Chrome Canary";
}

base::string16 GoogleChromeSxSDistribution::GetShortcutName() {
  return installer::GetLocalizedString(IDS_SXS_SHORTCUT_NAME_BASE);
}

base::string16 GoogleChromeSxSDistribution::GetStartMenuShortcutSubfolder(
    Subfolder subfolder_type) {
  switch (subfolder_type) {
    case SUBFOLDER_APPS:
      return installer::GetLocalizedString(
          IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY_BASE);
    default:
      DCHECK_EQ(subfolder_type, SUBFOLDER_CHROME);
      return GetShortcutName();
  }
}

bool GoogleChromeSxSDistribution::ShouldSetExperimentLabels() {
  return true;
}

bool GoogleChromeSxSDistribution::HasUserExperiments() {
  return true;
}
