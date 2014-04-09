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
enum MetricToken {
  // A cached token was successfully loaded from disk.
  kMetricTokenLoadSucceeded,
  // Reading a cached token from disk failed.
  kMetricTokenLoadFailed,

  // A token fetch request was sent to the DM server.
  kMetricTokenFetchRequested,
  // The request was invalid, or the HTTP request failed.
  kMetricTokenFetchRequestFailed,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricTokenFetchServerFailed,
  // A response to the fetch request was received.
  kMetricTokenFetchResponseReceived,
  // The response received was invalid. This happens when some expected data
  // was not present in the response.
  kMetricTokenFetchBadResponse,
  // DM server reported that management is not supported.
  kMetricTokenFetchManagementNotSupported,
  // DM server reported that the given device ID was not found.
  kMetricTokenFetchDeviceNotFound,
  // DM token successfully retrieved.
  kMetricTokenFetchOK,

  // Successfully cached a token to disk.
  kMetricTokenStoreSucceeded,
  // Caching a token to disk failed.
  kMetricTokenStoreFailed,

  // DM server reported that the device-id generated is not unique.
  kMetricTokenFetchDeviceIdConflict,
  // DM server reported that the serial number we try to register is invalid.
  kMetricTokenFetchInvalidSerialNumber,
  // DM server reported that the licenses for the domain have expired or been
  // exhausted.
  kMetricMissingLicenses,

  kMetricTokenSize  // Must be the last.
};

// Events related to fetching, saving and loading user and device policies.
enum MetricPolicy {
  // A cached policy was successfully loaded from disk.
  kMetricPolicyLoadSucceeded,
  // Reading a cached policy from disk failed.
  kMetricPolicyLoadFailed,

  // A policy fetch request was sent to the DM server.
  kMetricPolicyFetchRequested,
  // The request was invalid, or the HTTP request failed.
  kMetricPolicyFetchRequestFailed,
  // Error HTTP status received, or the DM server failed in another way.
  kMetricPolicyFetchServerFailed,
  // Policy not found for the given user or device.
  kMetricPolicyFetchNotFound,
  // DM server didn't accept the token used in the request.
  kMetricPolicyFetchInvalidToken,
  // A response to the policy fetch request was received.
  kMetricPolicyFetchResponseReceived,
  // The policy response message didn't contain a policy, or other data was
  // missing.
  kMetricPolicyFetchBadResponse,
  // Failed to decode the policy.
  kMetricPolicyFetchInvalidPolicy,
  // The device policy was rejected because its signature was invalid.
  kMetricPolicyFetchBadSignature,
  // Rejected policy because its timestamp is in the future.
  kMetricPolicyFetchTimestampInFuture,
  // Device policy rejected because the device is not managed.
  kMetricPolicyFetchNonEnterpriseDevice,
  // The policy was provided for a username that is different from the device
  // owner, and the policy was rejected.
  kMetricPolicyFetchUserMismatch,
  // The policy was rejected for another reason. Currently this can happen
  // only for device policies, when the SignedSettings fail to store or retrieve
  // a stored policy.
  kMetricPolicyFetchOtherFailed,
  // The fetched policy was accepted.
  kMetricPolicyFetchOK,
  // The policy just fetched didn't have any changes compared to the cached
  // policy.
  kMetricPolicyFetchNotModified,

  // Successfully cached a policy to disk.
  kMetricPolicyStoreSucceeded,
  // Caching a policy to disk failed.
  kMetricPolicyStoreFailed,

  kMetricPolicySize  // Must be the last.
};

// Events related to device enrollment.
enum MetricEnrollment {
  // The enrollment screen was closed without completing the enrollment
  // process.
  kMetricEnrollmentCancelled,
  // The user submitted credentials and started the enrollment process.
  kMetricEnrollmentStarted,
  // Enrollment failed due to a network error.
  kMetricEnrollmentNetworkFailed,
  // Enrollment failed because logging in to Gaia failed.
  kMetricEnrollmentLoginFailed,
  // Enrollment failed because it is not supported for the account used.
  kMetricEnrollmentNotSupported,
  // Enrollment failed because it failed to apply device policy.
  kMetricEnrollmentPolicyFailed,
  // Enrollment failed due to an unexpected error. This currently happens when
  // the Gaia auth token is not issued for the DM service, the device cloud
  // policy subsystem isn't initialized, or when fetching Gaia tokens fails
  // for an unknown reason.
  kMetricEnrollmentOtherFailed,
  // Enrollment was successful.
  kMetricEnrollmentOK,
  // Enrollment failed because the serial number we try to register is not
  // assigned to the domain used.
  kMetricEnrollmentInvalidSerialNumber,
  // Auto-enrollment started automatically after the user signed in.
  kMetricEnrollmentAutoStarted,
  // Auto-enrollment failed.
  kMetricEnrollmentAutoFailed,
  // Auto-enrollment was retried after having failed before.
  kMetricEnrollmentAutoRetried,
  // Auto-enrollment was canceled through the opt-out dialog.
  kMetricEnrollmentAutoCancelled,
  // Auto-enrollment succeeded.
  kMetricEnrollmentAutoOK,
  // Enrollment failed because the enrollment mode was not supplied by the
  // DMServer or the mode is not known to the client.
  kMetricEnrollmentInvalidEnrollmentMode,
  // Auto-enrollment is not supported for the mode supplied by the server.
  // This presently means trying to auto-enroll in kiosk mode.
  kMetricEnrollmentAutoEnrollmentNotSupported,
  // The lockbox initialization has taken too long to complete and the
  // enrollment has been canceled because of that.
  kMetricLockboxTimeoutError,
  // The username used to re-enroll the device does not belong to the domain
  // that the device was initially enrolled to.
  kMetricEnrollmentWrongUserError,
  // DM server reported that the licenses for the domain has expired or been
  // exhausted.
  kMetricMissingLicensesError,
  // Enrollment failed because the robot account auth code couldn't be
  // fetched from the DM Server.
  kMetricEnrollmentRobotAuthCodeFetchFailed,
  // Enrollment failed because the robot account auth code couldn't be
  // exchanged for a refresh token.
  kMetricEnrollmentRobotRefreshTokenFetchFailed,
  // Enrollment failed because the robot account refresh token couldn't be
  // persisted on the device.
  kMetricEnrollmentRobotRefreshTokenStoreFailed,
  // Enrollment failed because the administrator has deprovisioned the device.
  kMetricEnrollmentDeprovisioned,
  // Enrollment failed because the device doesn't belong to the domain.
  kMetricEnrollmentDomainMismatch,

  kMetricEnrollmentSize  // Must be the last.
};

// Events related to policy refresh.
enum MetricPolicyRefresh {
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were enabled.
  METRIC_POLICY_REFRESH_CHANGED,
  // A refresh occurred while the policy was not invalidated and the policy was
  // changed. Invalidations were disabled.
  METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS,
  // A refresh occurred while the policy was not invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_UNCHANGED,
  // A refresh occurred while the policy was invalidated and the policy was
  // changed.
  METRIC_POLICY_REFRESH_INVALIDATED_CHANGED,
  // A refresh occurred while the policy was invalidated and the policy was
  // unchanged.
  METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED,

  METRIC_POLICY_REFRESH_SIZE  // Must be the last.
};

// Types of policy invalidations.
enum PolicyInvalidationType {
  // The invalidation contained no payload.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD,
  // A normal invalidation containing a payload.
  POLICY_INVALIDATION_TYPE_NORMAL,
  // The invalidation contained no payload and was considered expired.
  POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED,
  // The invalidation contained a payload and was considered expired.
  POLICY_INVALIDATION_TYPE_EXPIRED,

  POLICY_INVALIDATION_TYPE_SIZE  // Must be the last.
};

// Names for the UMA counters. They are shared from here since the events
// from the same enum above can be triggered in different files, and must use
// the same UMA histogram name.
POLICY_EXPORT extern const char kMetricToken[];
POLICY_EXPORT extern const char kMetricPolicy[];
POLICY_EXPORT extern const char kMetricEnrollment[];
POLICY_EXPORT extern const char kMetricPolicyRefresh[];
POLICY_EXPORT extern const char kMetricPolicyInvalidations[];

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_ENTERPRISE_METRICS_H_
