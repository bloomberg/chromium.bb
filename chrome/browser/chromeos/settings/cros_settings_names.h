// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_

namespace chromeos {

extern const char kCrosSettingsPrefix[];

extern const char kAccountsPrefAllowGuest[];
extern const char kAccountsPrefAllowNewUser[];
extern const char kAccountsPrefShowUserNamesOnSignIn[];
extern const char kAccountsPrefUsers[];
extern const char kAccountsPrefEphemeralUsersEnabled[];
extern const char kAccountsPrefDeviceLocalAccounts[];
extern const char kAccountsPrefDeviceLocalAccountAutoLoginId[];
extern const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[];

extern const char kSettingProxyEverywhere[];

extern const char kSignedDataRoamingEnabled[];

extern const char kSystemTimezonePolicy[];
extern const char kSystemTimezone[];

extern const char kDeviceOwner[];

extern const char kStatsReportingPref[];

extern const char kReleaseChannel[];
extern const char kReleaseChannelDelegated[];

extern const char kReportDeviceVersionInfo[];
extern const char kReportDeviceActivityTimes[];
extern const char kReportDeviceBootMode[];
extern const char kReportDeviceLocation[];

extern const char kAppPack[];

extern const char kScreenSaverExtensionId[];
extern const char kScreenSaverTimeout[];

extern const char kIdleLogoutTimeout[];
extern const char kIdleLogoutWarningDuration[];

extern const char kStartUpUrls[];

extern const char kPolicyMissingMitigationMode[];

extern const char kAllowRedeemChromeOsRegistrationOffers[];

extern const char kStartUpFlags[];
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_
