// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PROVIDER_ANDROID_DELEGATE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PROVIDER_ANDROID_DELEGATE_H_

#include "base/compiler_specific.h"

namespace policy {

// A delegate for the Android policy provider. This class is responsible for
// setting policies on the PolicyProviderAndroid and refreshing them on demand.
class POLICY_EXPORT PolicyProviderAndroidDelegate {
 public:
  // Called to refresh policies. If this method is called, the delegate must
  // eventually call SetPolicies on the provider.
  virtual void RefreshPolicies() = 0;

  // Called before the provider is destroyed.
  virtual void PolicyProviderShutdown() = 0;

 protected:
  virtual ~PolicyProviderAndroidDelegate() {}
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PROVIDER_ANDROID_DELEGATE_H_
