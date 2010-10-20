// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_names.h"

namespace chromeos {

const char kCrosSettingsPrefix[] = "cros.";

const char kAccountsPrefAllowGuest[] = "cros.accounts.allowBWSI";
const char kAccountsPrefAllowNewUser[] = "cros.accounts.allowGuest";
const char kAccountsPrefShowUserNamesOnSignIn[]
    = "cros.accounts.showUserNamesOnSignIn";
const char kAccountsPrefUsers[] = "cros.accounts.users";

const char kSystemTimezone[] = "cros.system.timezone";

const char kDeviceOwner[] = "cros.device.owner";

const char kStatsReportingPref[] = "cros.metrics.reportingEnabled";

}  // namespace chromeos
