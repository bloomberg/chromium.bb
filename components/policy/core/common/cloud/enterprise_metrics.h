// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_

#include "components/policy/policy_export.h"

namespace policy {

// Metrics collected for enterprise events.

// Events related to fetching, saving and loading DM server tokens.
// These metrics are collected both for device and user tokens.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum MetricToken {
  // A cached token was successfully loaded from disk.
  kMetricTokenLoadSucceeded = 0,
  // Reading a cached token from disk failed.
  kMetricTokenLoadFailed = 1,

  // A token fetch request was sent to the DM server.
  kMetricTokenFetchRequested = 2,
  // The request was invalid, or the HTTP request failed.
  kMetricTokenFetchRequestFailed = 3,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricTokenFetchServerFailed = 4,
  // A response to the fetch request was received.
  kMetricTokenFetchResponseReceived = 5,
  // The response received was invalid. This happens when some expected data
  // was not present in the response.
  kMetricTokenFetchBadResponse = 6,
  // DM server reported that management is not supported.
  kMetricTokenFetchManagementNotSupported = 7,
  // DM server reported that the given device ID was not found.
  kMetricTokenFetchDeviceNotFound = 8,
  // DM token successfully retrieved.
  kMetricTokenFetchOK = 9,

  // Successfully cached a token to disk.
  kMetricTokenStoreSucceeded = 10,
  // Caching a token to disk failed.
  kMetricTokenStoreFailed = 11,

  // DM server reported that the device-id generated is not unique.
  kMetricTokenFetchDeviceIdConflict = 12,
  // DM server reported that the serial number we try to register is invalid.
  kMetricTokenFetchInvalidSerialNumber = 13,
  // DM server reported that the licenses for the domain have expired or been
  // exhausted.
  kMetricMissingLicenses = 14,

  kMetricTokenSize  // Must be the last.
};

// Events related to fetching, saving and loading user and device policies.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum MetricPolicy {
  // A cached policy was successfully loaded from disk.
  kMetricPolicyLoadSucceeded = 0,
  // Reading a cached policy from disk failed.
  kMetricPolicyLoadFailed = 1,

  // A policy fetch request was sent to the DM server.
  kMetricPolicyFetchRequested = 2,
  // The request was invalid, or the HTTP request failed.
  kMetricPolicyFetchRequestFailed = 3,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricPolicyFetchServerFailed = 4,
  // Policy not found for the given user or device.
  kMetricPolicyFetchNotFound = 5,
  // DM server didn't accept the token used in the request.
  kMetricPolicyFetchInvalidToken = 6,
  // A response to the policy fetch request was received.
  kMetricPolicyFetchResponseReceived = 7,
  // The policy response message didn't contain a policy, or other data was
  // missing.
  kMetricPolicyFetchBadResponse = 8,
  // Failed to decode the policy.
  kMetricPolicyFetchInvalidPolicy = 9,
  // The device policy was rejected because its signature was invalid.
  kMetricPolicyFetchBadSignature = 10,
  // Rejected policy because its timestamp is in the future.
  kMetricPolicyFetchTimestampInFuture = 11,
  // Device policy rejected because the device is not managed.
  kMetricPolicyFetchNonEnterpriseDevice = 12,
  // The policy was provided for a username that is different from the device
  // owner, and the policy was rejected.
  kMetricPolicyFetchUserMismatch = 13,
  // The policy was rejected for another reason. Currently this can happen
  // only for device policies, when the SignedSettings fail to store or retrieve
  // a stored policy.
  kMetricPolicyFetchOtherFailed = 14,
  // The fetched policy was accepted.
  kMetricPolicyFetchOK = 15,
  // The policy just fetched didn't have any changes compared to the cached
  // policy.
  kMetricPolicyFetchNotModified = 16,

  // Successfully cached a policy to disk.
  kMetricPolicyStoreSucceeded = 17,
  // Caching a policy to disk failed.
  kMetricPolicyStoreFailed = 18,

  kMetricPolicySize  // Must be the last.
};

// Events related to device enrollment.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum MetricEnrollment {
  // The enrollment screen was closed without completing the enrollment
  // process.
  kMetricEnrollmentCancelled = 0,
  // The user submitted credentials and started the enrollment process.
  kMetricEnrollmentStarted = 1,
  // Enrollment failed due to a network error.
  kMetricEnrollmentNetworkFailed = 2,
  // Enrollment failed because logging in to Gaia failed.
  kMetricEnrollmentLoginFailed = 3,
  // Enrollment failed because it is not supported for the account used.
  kMetricEnrollmentNotSupported = 4,
  // Enrollment failed because it failed to apply device policy.
  kMetricEnrollmentPolicyFailed = 5,
  // Enrollment failed due to an unexpected error. This currently happens when
  // the Gaia auth token is not issued for the DM service, the device cloud
  // policy subsystem isn't initialized, or when fetching Gaia tokens fails
  // for an unknown reason.
  kMetricEnrollmentOtherFailed = 6,
  // Enrollment was successful.
  kMetricEnrollmentOK = 7,
  // Enrollment failed because the serial number we try to register is not
  // assigned to the domain used.
  kMetricEnrollmentInvalidSerialNumber = 8,
  // Auto-enrollment started automatically after the user signed in.
  kMetricEnrollmentAutoStarted = 9,
  // Auto-enrollment failed.
  kMetricEnrollmentAutoFailed = 10,
  // Auto-enrollment was retried after having failed before.
  kMetricEnrollmentAutoRetried = 11,
  // Auto-enrollment was canceled through the opt-out dialog.
  kMetricEnrollmentAutoCancelled = 12,
  // Auto-enrollment succeeded.
  kMetricEnrollmentAutoOK = 13,
  // Enrollment failed because the enrollment mode was not supplied by the
  // DMServer or the mode is not known to the client.
  kMetricEnrollmentInvalidEnrollmentMode = 14,
  // Auto-enrollment is not supported for the mode supplied by the server.
  // This presently means trying to auto-enroll in kiosk mode.
  kMetricEnrollmentAutoEnrollmentNotSupported = 15,
  // The lockbox initialization has taken too long to complete and the
  // enrollment has been canceled because of that.
  kMetricLockboxTimeoutError = 16,
  // The username used to re-enroll the device does not belong to the domain
  // that the device was initially enrolled to.
  kMetricEnrollmentWrongUserError = 17,
  // DM server reported that the licenses for the domain has expired or been
  // exhausted.
  kMetricMissingLicensesError = 18,
  // Enrollment failed because the robot account auth code couldn't be
  // fetched from the DM Server.
  kMetricEnrollmentRobotAuthCodeFetchFailed = 19,
  // Enrollment failed because the robot account auth code couldn't be
  // exchanged for a refresh token.
  kMetricEnrollmentRobotRefreshTokenFetchFailed = 20,
  // Enrollment failed because the robot account refresh token couldn't be
  // persisted on the device.
  kMetricEnrollmentRobotRefreshTokenStoreFailed = 21,
  // Enrollment failed because the administrator has deprovisioned the device.
  kMetricEnrollmentDeprovisioned = 22,
  // Enrollment failed because the device doesn't belong to the domain.
  kMetricEnrollmentDomainMismatch = 23,
  // Enrollment has been triggered, the credential screen has been shown.
  kMetricEnrollmentTriggered = 24,
  // The user retried to submitted credentials.
  kMetricEnrollmentRetried = 25,

  kMetricEnrollmentSize  // Must be the last.
};

// Events related to policy refresh.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum MetricPolicyRefresh {
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were enabled.
  METRIC_POLICY_REFRESH_CHANGED = 0,
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were disabled.
  METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS = 1,
  // A refresh occurred while the policy was not invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_UNCHANGED = 2,
  // A refresh occurred while the policy was invalidated and the policy was
  // changed.
  METRIC_POLICY_REFRESH_INVALIDATED_CHANGED = 3,
  // A refresh occurred while the policy was invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED = 4,

  METRIC_POLICY_REFRESH_SIZE  // Must be the last.
};

// Types of policy invalidations.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum PolicyInvalidationType {
  // The invalidation contained no payload.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD = 0,
  // A normal invalidation containing a payload.
  POLICY_INVALIDATION_TYPE_NORMAL = 1,
  // The invalidation contained no payload and was considered expired.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED = 3,
  // The invalidation contained a payload and was considered expired.
  POLICY_INVALIDATION_TYPE_EXPIRED = 4,

  POLICY_INVALIDATION_TYPE_SIZE  // Must be the last.
};

// Names for the UMA counters. They are shared from here since the events
// from the same enum above can be triggered in different files, and must use
// the same UMA histogram name.
POLICY_EXPORT extern const char kMetricToken[];
POLICY_EXPORT extern const char kMetricPolicy[];
POLICY_EXPORT extern const char kMetricEnrollment[];
POLICY_EXPORT extern const char kMetricEnrollmentRecovery[];
POLICY_EXPORT extern const char kMetricPolicyRefresh[];
POLICY_EXPORT extern const char kMetricPolicyInvalidations[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
