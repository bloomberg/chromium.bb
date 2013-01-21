// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_constants.h"

namespace policy {

// Constants related to the device management protocol.
namespace dm_protocol {

// Name constants for URL query parameters.
const char kParamAgent[] = "agent";
const char kParamAppType[] = "apptype";
const char kParamDeviceID[] = "deviceid";
const char kParamDeviceType[] = "devicetype";
const char kParamOAuthToken[] = "oauth_token";
const char kParamPlatform[] = "platform";
const char kParamRequest[] = "request";
const char kParamUserAffiliation[] = "user_affiliation";

// String constants for the device and app type we report to the server.
const char kValueAppType[] = "Chrome";
const char kValueDeviceType[] = "2";
const char kValueRequestAutoEnrollment[] = "enterprise_check";
const char kValueRequestPolicy[] = "policy";
const char kValueRequestRegister[] = "register";
const char kValueRequestUnregister[] = "unregister";
const char kValueUserAffiliationManaged[] = "managed";
const char kValueUserAffiliationNone[] = "none";

const char kChromeDevicePolicyType[] = "google/chromeos/device";
#if defined(OS_CHROMEOS)
const char kChromeUserPolicyType[] = "google/chromeos/user";
#else
const char kChromeUserPolicyType[] = "google/chrome/user";
#endif
const char kChromePublicAccountPolicyType[] = "google/chromeos/publicaccount";
const char kChromeExtensionPolicyType[] = "google/chrome/extension";

}  // namespace dm_protocol

}  // namespace policy
