// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_

namespace authpolicy {
const char kAuthPolicyInterface[] = "org.chromium.AuthPolicy";
const char kAuthPolicyServicePath[] = "/org/chromium/AuthPolicy";
const char kAuthPolicyServiceName[] = "org.chromium.AuthPolicy";
// Methods
const char kAuthPolicyAuthenticateUser[] = "AuthenticateUser";
const char kAuthPolicyJoinADDomain[] = "JoinADDomain";
const char kAuthPolicyRefreshUserPolicy[] = "RefreshUserPolicy";
const char kAuthPolicyRefreshDevicePolicy[] = "RefreshDevicePolicy";

// Enum values.
enum ADJoinErrorType {
  AD_JOIN_ERROR_NONE = 0,
  AD_JOIN_ERROR_UNKNOWN = 1,
  AD_JOIN_ERROR_DBUS_FAILURE = 2,
};

enum AuthUserErrorType {
  AUTH_USER_ERROR_NONE = 0,
  AUTH_USER_ERROR_UNKNOWN = 1,
  AUTH_USER_ERROR_DBUS_FAILURE = 2,
};

enum RefreshUserPolicyErrorType {
  REFRESH_USER_POLICY_ERROR_NONE = 0,
  REFRESH_USER_POLICY_ERROR_UNKNOWN = 1,
  REFRESH_USER_POLICY_ERROR_DBUS_FAILURE = 2,
};

enum RefreshDevicePolicyErrorType {
  REFRESH_DEVICE_POLICY_ERROR_NONE = 0,
  REFRESH_DEVICE_POLICY_ERROR_UNKNOWN = 1,
  REFRESH_DEVICE_POLICY_ERROR_DBUS_FAILURE = 2,
};
}  // namespace authpolicy

#endif  // SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
