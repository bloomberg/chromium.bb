// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/settings/cros_settings_names.h"

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
const char kAccountsPrefDeviceLocalAccountsKeyId[] =
    "id";
const char kAccountsPrefDeviceLocalAccountsKeyType[] =
    "type";
const char kAccountsPrefDeviceLocalAccountsKeyKioskAppId[] =
    "kiosk_app_id";
const char kAccountsPrefDeviceLocalAccountAutoLoginId[] =
    "cros.accounts.deviceLocalAccountAutoLoginId";
const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[] =
    "cros.accounts.deviceLocalAccountAutoLoginDelay";
const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[] =
    "cros.accounts.deviceLocalAccountAutoLoginBailoutEnabled";
const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[] =
    "cros.accounts.deviceLocalAccountPromptForNetworkWhenOffline";
const char kAccountsPrefSupervisedUsersEnabled[] =
    "cros.accounts.supervisedUsersEnabled";

// All cros.signed.* settings are stored in SignedSettings.
const char kSignedDataRoamingEnabled[] = "cros.signed.data_roaming_enabled";

// True if auto-update was disabled by the system administrator.
const char kUpdateDisabled[] = "cros.system.updateDisabled";

// A list of strings which specifies allowed connection types for
// update.
const char kAllowedConnectionTypesForUpdate[] =
    "cros.system.allowedConnectionTypesForUpdate";

// The first constant refers to the user setting editable in the UI. The second
// refers to the timezone policy. This seperation is necessary to allow the user
// to temporarily change the timezone for the current session and reset it to
// the policy's value on logout.
const char kSystemTimezone[] = "cros.system.timezone";
const char kSystemTimezonePolicy[] = "cros.system.timezone_policy";

// Value of kUse24HourClock user preference of device' owner.
// ChromeOS device uses this setting on login screen.
const char kSystemUse24HourClock[] = "cros.system.use_24hour_clock";

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

// Determines whether the device reports network interface types and addresses
// in device status reports to the device management server.
const char kReportDeviceNetworkInterfaces[] =
    "cros.device_status.report_network_interfaces";

// Determines whether the device reports recently logged in users in device
// status reports to the device management server.
const char kReportDeviceUsers[] = "cros.device_status.report_users";

// A list of dictionaries, each detailing one extension to install as part of
// the AppPack and including the following fields:
// "extension-id": ID of the extension to install
// "update-url": URL to check the extension's version and download location
// "key-checksum": checksum of the extension's CRX public key, encoded in hex.
const char kAppPack[] = "cros.app_pack";
const char kAppPackKeyExtensionId[] = "extension-id";
const char kAppPackKeyUpdateUrl[] = "update-url";

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

// A string pref for the restrict parameter to be appended to the Variations URL
// when pinging the Variations server.
const char kVariationsRestrictParameter[] =
    "cros.variations_restrict_parameter";

// A boolean pref that indicates whether enterprise attestation is enabled for
// the device.
const char kDeviceAttestationEnabled[] = "cros.device.attestation_enabled";

// A boolean pref that indicates whether attestation for content protection is
// enabled for the device.
const char kAttestationForContentProtectionEnabled[] =
    "cros.device.attestation_for_content_protection_enabled";

// The service account identity for device-level service accounts on
// enterprise-enrolled devices.
const char kServiceAccountIdentity[] = "cros.service_account_identity";

}  // namespace chromeos
