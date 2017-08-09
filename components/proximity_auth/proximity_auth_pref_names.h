// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H

namespace proximity_auth {
namespace prefs {

extern const char kEasyUnlockAllowed[];
extern const char kEasyUnlockEnabled[];
extern const char kEasyUnlockProximityThreshold[];
extern const char kEasyUnlockLocalStateUserPrefs[];
extern const char kProximityAuthLastPasswordEntryTimestampMs[];
extern const char kProximityAuthLastPromotionCheckTimestampMs[];
extern const char kProximityAuthPromotionShownCount[];
extern const char kProximityAuthRemoteBleDevices[];
extern const char kProximityAuthIsChromeOSLoginEnabled[];

}  // namespace prefs
}  // proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H
