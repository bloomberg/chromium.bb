// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_PREF_NAMES_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_PREF_NAMES_H

namespace proximity_auth {
namespace prefs {

extern const char kCryptAuthDeviceSyncLastSyncTimeSeconds[];
extern const char kCryptAuthDeviceSyncIsRecoveringFromFailure[];
extern const char kCryptAuthDeviceSyncReason[];
extern const char kCryptAuthDeviceSyncUnlockKeys[];
extern const char kCryptAuthEnrollmentIsRecoveringFromFailure[];
extern const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[];
extern const char kCryptAuthEnrollmentReason[];
extern const char kCryptAuthEnrollmentUserPublicKey[];
extern const char kCryptAuthEnrollmentUserPrivateKey[];
extern const char kCryptAuthGCMRegistrationId[];

}  // namespace prefs
}  // proximity_auth

#endif  // COMPONENTS_PROXIMITY_CRYPTAUTH_PREF_NAMES_H
