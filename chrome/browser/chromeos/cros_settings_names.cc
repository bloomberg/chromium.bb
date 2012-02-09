// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_names.h"

namespace chromeos {

const char kCrosSettingsPrefix[] = "cros.";

// All cros.accounts.* settings are stored in SignedSettings.
const char kAccountsPrefAllowGuest[] = "cros.accounts.allowBWSI";
const char kAccountsPrefAllowNewUser[] = "cros.accounts.allowGuest";
const char kAccountsPrefShowUserNamesOnSignIn[]
    = "cros.accounts.showUserNamesOnSignIn";
const char kAccountsPrefUsers[] = "cros.accounts.users";
const char kAccountsPrefEphemeralUsers[] = "cros.accounts.ephemeralUsers";

// Name of signed setting persisted on device, writeable only by owner.
const char kSettingProxyEverywhere[] = "cros.proxy.everywhere";

// All cros.signed.* settings are stored in SignedSettings.
const char kSignedDataRoamingEnabled[] = "cros.signed.data_roaming_enabled";

const char kSystemTimezone[] = "cros.system.timezone";

const char kDeviceOwner[] = "cros.device.owner";

const char kStatsReportingPref[] = "cros.metrics.reportingEnabled";

const char kReleaseChannel[] = "cros.system.releaseChannel";

// A boolean pref that indicates whether OS & firmware version info should be
// reported along with device policy requests.
const char kReportDeviceVersionInfo[] =
    "cros.device_status.report_version_info";

// A boolean pref that indicates whether device activity times should be
// recorded and reported along with device policy requests.
const char kReportDeviceActivityTimes[] =
    "cros.device_status.report_activity_times";

// A boolean pref that indicates whether device the state of the dev switch
// at boot mode should be reported along with device policy requests.
const char kReportDeviceBootMode[] = "cros.device_status.report_boot_mode";

}  // namespace chromeos
