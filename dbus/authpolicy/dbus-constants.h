// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_

namespace authpolicy {

// General
const char kAuthPolicyInterface[] = "org.chromium.AuthPolicy";
const char kAuthPolicyServicePath[] = "/org/chromium/AuthPolicy";
const char kAuthPolicyServiceName[] = "org.chromium.AuthPolicy";

// TODO(ljusten): Remove old naming style once Chrome uses the new style.

// Old naming style (please don't use).

// Methods
const char kAuthPolicyAuthenticateUser[] = "AuthenticateUser";
const char kAuthPolicyGetUserStatus[] = "GetUserStatus";
const char kAuthPolicyJoinADDomain[] = "JoinADDomain";
const char kAuthPolicyRefreshUserPolicy[] = "RefreshUserPolicy";
const char kAuthPolicyRefreshDevicePolicy[] = "RefreshDevicePolicy";

// New naming style.

// Methods
const char kAuthenticateUserMethod[] = "AuthenticateUser";
const char kGetUserStatusMethod[] = "GetUserStatus";
const char kGetUserKerberosFilesMethod[] = "GetUserKerberosFiles";
const char kJoinADDomainMethod[] = "JoinADDomain";
const char kRefreshUserPolicyMethod[] = "RefreshUserPolicy";
const char kRefreshDevicePolicyMethod[] = "RefreshDevicePolicy";

// Signals
const char kUserKerberosFilesChangedSignal[] = "UserKerberosFilesChanged";

} // namespace authpolicy

#endif // SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
