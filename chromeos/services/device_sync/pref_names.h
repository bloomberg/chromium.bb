// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PREF_NAMES_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PREF_NAMES_H_

namespace chromeos {

namespace device_sync {

namespace prefs {

extern const char kCryptAuthDeviceSyncLastSyncTimeSeconds[];
extern const char kCryptAuthDeviceSyncIsRecoveringFromFailure[];
extern const char kCryptAuthDeviceSyncReason[];
extern const char kCryptAuthDeviceSyncUnlockKeys[];
extern const char kCryptAuthEnrollmentFailureRecoveryInvocationReason[];
extern const char kCryptAuthEnrollmentFailureRecoverySessionId[];
extern const char kCryptAuthEnrollmentIsRecoveringFromFailure[];
extern const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[];
extern const char kCryptAuthEnrollmentReason[];
extern const char kCryptAuthEnrollmentSchedulerClientDirective[];
extern const char kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime[];
extern const char kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime[];
extern const char kCryptAuthEnrollmentSchedulerNumConsecutiveFailures[];
extern const char kCryptAuthEnrollmentUserPublicKey[];
extern const char kCryptAuthEnrollmentUserPrivateKey[];
extern const char kCryptAuthGCMRegistrationId[];
extern const char kCryptAuthKeyRegistry[];

}  // namespace prefs

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PREF_NAMES_H_
