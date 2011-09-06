// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ENTERPRISE_METRICS_H_
#define CHROME_BROWSER_POLICY_ENTERPRISE_METRICS_H_
#pragma once

namespace policy {

// Metrics collected for enterprise events.

// Events related to fetching, saving and loading DM server tokens.
// These metrics are collected both for device and user tokens.
enum MetricToken {
  // A cached token was successfully loaded from disk.
  kMetricTokenLoadSucceeded = 0,
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

  kMetricTokenSize  // Must be the last.
};

// Events related to fetching, saving and loading user and device policies.
enum MetricPolicy {
  // A cached policy was successfully loaded from disk.
  kMetricPolicyLoadSucceeded = 0,
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
  kMetricEnrollmentCancelled = 0,
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

  kMetricEnrollmentSize  // Must be the last.
};

// Names for the UMA counters. They are shared from here since the events
// from the same enum above can be triggered in different files, and must use
// the same UMA histogram name.
extern const char* kMetricToken;
extern const char* kMetricPolicy;
extern const char* kMetricEnrollment;

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ENTERPRISE_METRICS_H_
