// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines dummy implementation of several functions from the
// BrowserDistribution class for Google Chrome. These functions allow 64-bit
// Windows Chrome binary to build successfully. Since this binary is only used
// for Native Client support, most of the install/uninstall functionality is not
// necessary there.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <utility>

#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(
          std::make_unique<NonUpdatingAppRegistrationData>(base::string16()));
