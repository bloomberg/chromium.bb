// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_switches.h"

namespace policy {
namespace switches {

// Specifies the URL at which to fetch configuration policy from the device
// management backend.
const char kDeviceManagementUrl[]           = "device-management-url";

// Disables fetching and storing cloud policy for components.
const char kDisableComponentCloudPolicy[]   = "disable-component-cloud-policy";

// Disables the verification of policy signing keys.
// TODO(atwilson): Remove this once all test servers have been updated to
// produce verification signatures.
const char kDisablePolicyKeyVerification[]  = "disable-policy-key-verification";

#if defined(OS_ANDROID) || defined(OS_IOS)
// Registers for cloud policy using the BROWSER client type instead of the
// ANDROID_BROWSER or IOS_BROWSER types.
// This allows skipping the server whitelist.
// TODO(joaodasilva): remove this. http://crbug.com/248527
const char kFakeCloudPolicyType[]           = "fake-cloud-policy-type";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace switches
}  // namespace policy
