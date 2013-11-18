// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_switches.h"

namespace policy {
namespace switches {

// The maximum amount of delay in ms between receiving a cloud policy
// invalidation and fetching the policy. A random delay up to this value is used
// to prevent Chrome clients from overwhelming the cloud policy server when a
// policy which affects many users is changed.
const char kCloudPolicyInvalidationDelay[]  = "cloud-policy-invalidation-delay";

// Disables pushing cloud policy to Chrome using an invalidation service.
const char kDisableCloudPolicyPush[]        = "disable-cloud-policy-push";

// Enables fetching and storing cloud policy for components. This currently
// supports policy for extensions on Chrome OS.
const char kEnableComponentCloudPolicy[]    = "enable-component-cloud-policy";

#if defined(OS_ANDROID) || defined(OS_IOS)
// Registers for cloud policy using the BROWSER client type instead of the
// ANDROID_BROWSER or IOS_BROWSER types.
// This allows skipping the server whitelist.
// TODO(joaodasilva): remove this. http://crbug.com/248527
const char kFakeCloudPolicyType[]           = "fake-cloud-policy-type";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace switches
}  // namespace policy
