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

// Preference name for the preference which stores the ID corresponding to
// the device which has most recently replied to a ConnectTetheringRequest with
// a response code indicating that its hotspot has started up successfully.
extern const char kMostRecentConnectTetheringResponderId[];

}  // namespace prefs

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_PREF_NAMES_H_
