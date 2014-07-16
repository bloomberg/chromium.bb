// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_

#include <string>
#include <utility>

#include "components/policy/policy_export.h"

namespace policy {

// Constants related to the device management protocol.
namespace dm_protocol {

// Name extern constants for URL query parameters.
POLICY_EXPORT extern const char kParamAgent[];
POLICY_EXPORT extern const char kParamAppType[];
POLICY_EXPORT extern const char kParamDeviceID[];
POLICY_EXPORT extern const char kParamDeviceType[];
POLICY_EXPORT extern const char kParamOAuthToken[];
POLICY_EXPORT extern const char kParamPlatform[];
POLICY_EXPORT extern const char kParamRequest[];
POLICY_EXPORT extern const char kParamUserAffiliation[];

// String extern constants for the device and app type we report to the server.
POLICY_EXPORT extern const char kValueAppType[];
POLICY_EXPORT extern const char kValueDeviceType[];
POLICY_EXPORT extern const char kValueRequestAutoEnrollment[];
POLICY_EXPORT extern const char kValueRequestPolicy[];
POLICY_EXPORT extern const char kValueRequestRegister[];
POLICY_EXPORT extern const char kValueRequestApiAuthorization[];
POLICY_EXPORT extern const char kValueRequestUnregister[];
POLICY_EXPORT extern const char kValueRequestUploadCertificate[];
POLICY_EXPORT extern const char kValueRequestDeviceStateRetrieval[];
POLICY_EXPORT extern const char kValueUserAffiliationManaged[];
POLICY_EXPORT extern const char kValueUserAffiliationNone[];

// Policy type strings for the policy_type field in PolicyFetchRequest.
POLICY_EXPORT extern const char kChromeDevicePolicyType[];
POLICY_EXPORT extern const char kChromeUserPolicyType[];
POLICY_EXPORT extern const char kChromePublicAccountPolicyType[];
POLICY_EXPORT extern const char kChromeExtensionPolicyType[];

// These codes are sent in the |error_code| field of PolicyFetchResponse.
enum PolicyFetchStatus {
  POLICY_FETCH_SUCCESS = 200,
  POLICY_FETCH_ERROR_NOT_FOUND = 902,
};

}  // namespace dm_protocol

// The header used to transmit the policy ID for this client.
POLICY_EXPORT extern const char kChromePolicyHeader[];

// Information about the verification key used to verify that policy signing
// keys are valid.
POLICY_EXPORT std::string GetPolicyVerificationKey();
POLICY_EXPORT extern const char kPolicyVerificationKeyHash[];

// Describes the affiliation of a user w.r.t. the device owner.
enum UserAffiliation {
  // User is on the same domain the device was registered with.
  USER_AFFILIATION_MANAGED,
  // No affiliation between device and user.
  USER_AFFILIATION_NONE,
};

// Status codes for communication errors with the device management service.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DeviceManagementStatus {
  // All is good.
  DM_STATUS_SUCCESS = 0,
  // Request payload invalid.
  DM_STATUS_REQUEST_INVALID = 1,
  // The HTTP request failed.
  DM_STATUS_REQUEST_FAILED = 2,
  // The server returned an error code that points to a temporary problem.
  DM_STATUS_TEMPORARY_UNAVAILABLE = 3,
  // The HTTP request returned a non-success code.
  DM_STATUS_HTTP_STATUS_ERROR = 4,
  // Response could not be decoded.
  DM_STATUS_RESPONSE_DECODING_ERROR = 5,
  // Service error: Management not supported.
  DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED = 6,
  // Service error: Device not found.
  DM_STATUS_SERVICE_DEVICE_NOT_FOUND = 7,
  // Service error: Device token invalid.
  DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID = 8,
  // Service error: Activation pending.
  DM_STATUS_SERVICE_ACTIVATION_PENDING = 9,
  // Service error: The serial number is not valid or not known to the server.
  DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER = 10,
  // Service error: The device id used for registration is already taken.
  DM_STATUS_SERVICE_DEVICE_ID_CONFLICT = 11,
  // Service error: The licenses have expired or have been exhausted.
  DM_STATUS_SERVICE_MISSING_LICENSES = 12,
  // Service error: The administrator has deprovisioned this client.
  DM_STATUS_SERVICE_DEPROVISIONED = 13,
  // Service error: Device registration for the wrong domain.
  DM_STATUS_SERVICE_DOMAIN_MISMATCH = 14,
  // Service error: Policy not found. Error code defined by the DM folks.
  DM_STATUS_SERVICE_POLICY_NOT_FOUND = 902,
};

// List of modes that the device can be locked into.
enum DeviceMode {
  DEVICE_MODE_PENDING,         // The device mode is not yet available.
  DEVICE_MODE_NOT_SET,         // The device is not yet enrolled or owned.
  DEVICE_MODE_CONSUMER,        // The device is locally owned as consumer
                               // device.
  DEVICE_MODE_ENTERPRISE,      // The device is enrolled as an enterprise
                               // device.
  DEVICE_MODE_RETAIL_KIOSK,    // The device is enrolled as retail kiosk device.
  DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,  // The device is locally owned as
                                          // consumer kiosk with ability to auto
                                          // launch a kiosk webapp.
};

// A pair that combines a policy fetch type and entity ID.
typedef std::pair<std::string, std::string> PolicyNamespaceKey;

// Returns the Chrome user policy type to use. This allows overridding the
// default user policy type on Android and iOS for testing purposes.
// TODO(joaodasilva): remove this once the server is ready.
// http://crbug.com/248527
POLICY_EXPORT const char* GetChromeUserPolicyType();

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CONSTANTS_H_
