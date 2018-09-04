// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <memory>

#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/updating_app_registration_data.h"

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(std::make_unique<UpdatingAppRegistrationData>(
          install_static::GetAppGuid())) {}

base::string16 GoogleChromeDistribution::GetPublisherName() {
  const base::string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}
