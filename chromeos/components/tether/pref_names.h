// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_PREF_NAMES_H_
#define CHROMEOS_COMPONENTS_TETHER_PREF_NAMES_H_

namespace chromeos {

namespace tether {

namespace prefs {

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a TetherAvailabilityRequest with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
extern const char kMostRecentTetherAvailablilityResponderIds[];

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a ConnectTetheringResponse with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
extern const char kMostRecentConnectTetheringResponderIds[];

// The status of the active host. The value stored for this key is the integer
// version of an ActiveHost::ActiveHostStatus enumeration value.
extern const char kActiveHostStatus[];

// The device ID of the active host. If there is no active host, the value at
// this key is "".
extern const char kActiveHostDeviceId[];

// The tether network GUID of the active host. If there is no active host, the
// value at this key is "".
extern const char kTetherNetworkGuid[];

// The Wi-Fi network GUID of the active host. If there is no active host, the
// value at this key is "".
extern const char kWifiNetworkGuid[];

}  // namespace prefs

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_PREF_NAMES_H_
