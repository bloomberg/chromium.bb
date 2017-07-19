// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_pref_names.h"

namespace proximity_auth {
namespace prefs {

// The timestamp of the last password entry in milliseconds, used to enforce
// reauthing with the password after a given time period has elapsed.
const char kProximityAuthLastPasswordEntryTimestampMs[] =
    "proximity_auth.last_password_entry_timestamp_ms";

// The timestamp of the last promotion check in milliseconds.
const char kProximityAuthLastPromotionCheckTimestampMs[] =
    "proximity_auth.last_promotion_check_timestamp_ms";

// The dictionary containing remote BLE devices.
const char kProximityAuthRemoteBleDevices[] =
    "proximity_auth.remote_ble_devices";

// How close the remote and the local device should be in order to use Easy
// Unlock.
const char kEasyUnlockProximityThreshold[] = "easy_unlock.proximity_threshold";

// Whether or not EasyUnlock is enabled on the ChromeOS login screen (in
// addition to the lock screen).
extern const char kProximityAuthIsChromeOSLoginEnabled[] =
    "proximity_auth.is_chromeos_login_enabled";

}  // namespace prefs
}  // namespace proximity_auth
