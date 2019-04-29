// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/pref_names.h"

namespace chromeos {

namespace device_sync {

namespace prefs {

// Whether the system is scheduling device_syncs more aggressively to recover
// from the previous device_sync failure.
const char kCryptAuthDeviceSyncIsRecoveringFromFailure[] =
    "cryptauth.device_sync.is_recovering_from_failure";

// The timestamp of the last successful CryptAuth device_sync in seconds.
const char kCryptAuthDeviceSyncLastSyncTimeSeconds[] =
    "cryptauth.device_sync.last_device_sync_time_seconds";

// The reason that the next device_sync is performed. This should be one of the
// enum values of cryptauth::InvocationReason in
// chromeos/services/device_sync/proto/cryptauth_api.proto.
const char kCryptAuthDeviceSyncReason[] = "cryptauth.device_sync.reason";

// A list of unlock keys (stored as dictionaries) synced from CryptAuth. Unlock
// Keys are phones belonging to the user that can unlock other devices, such as
// desktop PCs.
const char kCryptAuthDeviceSyncUnlockKeys[] =
    "cryptauth.device_sync.unlock_keys";

// The CryptAuth v2 Enrollment invocation reason to use when retrying a failed
// enrollment attempt. This should be one of the enum values of
// cryptauth2::ClientMetadata::InvocationReason in
// chromeos/services/device_sync/proto/cryptauth_common.proto.
const char kCryptAuthEnrollmentFailureRecoveryInvocationReason[] =
    "cryptauth.enrollment.failure_recovery_invocation_reason";

// The session ID, sent via a GCM message, to use when retrying a failed
// enrollment attempt.
const char kCryptAuthEnrollmentFailureRecoverySessionId[] =
    "cryptauth.enrollment.failure_recovery_session_id";

// Whether the system is scheduling enrollments more aggressively to recover
// from the previous enrollment failure.
const char kCryptAuthEnrollmentIsRecoveringFromFailure[] =
    "cryptauth.enrollment.is_recovering_from_failure";

// The timestamp of the last successful CryptAuth enrollment in seconds.
const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[] =
    "cryptauth.enrollment.last_enrollment_time_seconds";

// The reason that the next enrollment is performed. This should be one of the
// enum values of cryptauth::InvocationReason in
// chromeos/services/device_sync/proto/cryptauth_api.proto.
const char kCryptAuthEnrollmentReason[] = "cryptauth.enrollment.reason";

// The most recent ClientDirective--serialized to a string and base64
// encoded--sent to the CryptAuthEnrollmentScheduler.
const char kCryptAuthEnrollmentSchedulerClientDirective[] =
    "cryptauth.enrollment.scheduler_client_directive";

// The time of the last enrollment attempt.
const char kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime[] =
    "cryptauth.enrollment.scheduler_last_enrollment_attempt_time";

// The time of the last successful enrollment.
const char kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime[] =
    "cryptauth.enrollment.scheduler_last_successful_enrollment_time";

// The number of failed enrollments since the last successful enrollment.
const char kCryptAuthEnrollmentSchedulerNumConsecutiveFailures[] =
    "cryptauth.enrollment.scheduler_num_consecutive_failures";

// The public key of the user and device enrolled with CryptAuth.
const char kCryptAuthEnrollmentUserPublicKey[] =
    "cryptauth.enrollment.user_public_key";

// The private key of the user and device enrolled with CryptAuth.
const char kCryptAuthEnrollmentUserPrivateKey[] =
    "cryptauth.enrollment.user_private_key";

// The GCM registration id used for receiving push messages from CryptAuth.
const char kCryptAuthGCMRegistrationId[] = "cryptauth.gcm_registration_id";

// The dictionary of key bundles enrolled with CryptAuth, used to populate and
// persist the CryptAuthKeyRegistry.
const char kCryptAuthKeyRegistry[] = "cryptauth.key_registry";

}  // namespace prefs

}  // namespace device_sync

}  // namespace chromeos
