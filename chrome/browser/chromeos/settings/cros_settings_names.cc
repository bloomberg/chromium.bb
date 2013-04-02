// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/cros_settings_names.h"

#include "base/basictypes.h"

namespace chromeos {

const char kCrosSettingsPrefix[] = "cros.";

// All cros.accounts.* settings are stored in SignedSettings.
const char kAccountsPrefAllowGuest[] = "cros.accounts.allowBWSI";
const char kAccountsPrefAllowNewUser[] = "cros.accounts.allowGuest";
const char kAccountsPrefShowUserNamesOnSignIn[]
    = "cros.accounts.showUserNamesOnSignIn";
const char kAccountsPrefUsers[] = "cros.accounts.users";
const char kAccountsPrefEphemeralUsersEnabled[] =
    "cros.accounts.ephemeralUsersEnabled";
const char kAccountsPrefDeviceLocalAccounts[] =
    "cros.accounts.deviceLocalAccounts";
const char kAccountsPrefDeviceLocalAccountAutoLoginId[] =
    "cros.accounts.deviceLocalAccountAutoLoginId";
const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[] =
    "cros.accounts.deviceLocalAccountAutoLoginDelay";

// Name of signed setting persisted on device, writeable only by owner.
const char kSettingProxyEverywhere[] = "cros.proxy.everywhere";

// All cros.signed.* settings are stored in SignedSettings.
const char kSignedDataRoamingEnabled[] = "cros.signed.data_roaming_enabled";

// The first constant refers to the user setting editable in the UI. The second
// refers to the timezone policy. This seperation is necessary to allow the user
// to temporarily change the timezone for the current session and reset it to
// the policy's value on logout.
const char kSystemTimezone[] = "cros.system.timezone";
const char kSystemTimezonePolicy[] = "cros.system.timezone_policy";

const char kDeviceOwner[] = "cros.device.owner";

const char kStatsReportingPref[] = "cros.metrics.reportingEnabled";

const char kReleaseChannel[] = "cros.system.releaseChannel";
const char kReleaseChannelDelegated[] = "cros.system.releaseChannelDelegated";

// A boolean pref that indicates whether OS & firmware version info should be
// reported along with device policy requests.
const char kReportDeviceVersionInfo[] =
    "cros.device_status.report_version_info";

// A boolean pref that indicates whether device activity times should be
// recorded and reported along with device policy requests.
const char kReportDeviceActivityTimes[] =
    "cros.device_status.report_activity_times";

// A boolean pref that indicates whether the state of the dev mode switch at
// boot should be reported along with device policy requests.
const char kReportDeviceBootMode[] = "cros.device_status.report_boot_mode";

// A boolean pref that indicates whether the current location should be reported
// along with device policy requests.
const char kReportDeviceLocation[] = "cros.device_status.report_location";

// A list of dictionaries, each detailing one extension to install as part of
// the AppPack and including the following fields:
// "extension-id": ID of the extension to install
// "update-url": URL to check the extension's version and download location
// "key-checksum": checksum of the extension's CRX public key, encoded in hex.
const char kAppPack[] = "cros.app_pack";

// Values from the ScreenSaver proto. Defines the extension ID of the screen
// saver extension and the timeout before the screen saver should be started.
const char kScreenSaverExtensionId[] = "cros.screen_saver.extension_id";
const char kScreenSaverTimeout[] = "cros.screen_saver.timeout";

// Values from the ForcedLogoutTimeouts proto. Defines the timeouts before a
// user is logged out after some period of inactivity as well as the duration of
// a warning message informing the user about the pending logout.
const char kIdleLogoutTimeout[] = "cros.idle_logout.timeout";
const char kIdleLogoutWarningDuration[] = "cros.idle_logout.warning_duration";

// Defines the set of URLs to be opened on login to the anonymous account used
// if the device is in KIOSK mode.
const char kStartUpUrls[] = "cros.start_up_urls";

// This policy should not appear in the protobuf ever but is used internally to
// signal that we are running in a "safe-mode" for policy recovery.
const char kPolicyMissingMitigationMode[] =
    "cros.internal.policy_mitigation_mode";

// A boolean pref that indicates whether users are allowed to redeem offers
// through Chrome OS Registration.
const char kAllowRedeemChromeOsRegistrationOffers[] =
    "cros.echo.allow_redeem_chrome_os_registration_offers";

// A list pref storing the flags that need to be applied to the browser upon
// start-up.
const char kStartUpFlags[] = "cros.startup_flags";

// A prefix for all kiosk app setting names.
const char kKioskAppSettingsPrefix[] = "cros.kiosk.";
const int kKioskAppSettingsPrefixLength =
    arraysize(kKioskAppSettingsPrefix) - 1;  // exclude NULL terminator.

// A list of available kiosk application IDs.
const char kKioskApps[] = "cros.kiosk.apps";

// An app id string of the current auto launch app. Empty string means no auto
// launch app. Default is empty.
const char kKioskAutoLaunch[] = "cros.kiosk.auto_launch";

// A boolean pref of whether the app launch bailout shortcut should be disabled.
// When set to true, the bailout shortcut is disabled. Default is false.
const char kKioskDisableBailoutShortcut[] =
    "cros.kiosk.disable_bailout_shortcut";

}  // namespace chromeos
