// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_

#include "base/basictypes.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_export.h"

namespace policy {

// A policy provider implementation that acts as a proxy for another policy
// provider, swappable at any point.
class POLICY_EXPORT ProxyPolicyProvider
    : public ConfigurationPolicyProvider,
      public ConfigurationPolicyProvider::Observer {
 public:
  ProxyPolicyProvider();
  virtual ~ProxyPolicyProvider();

  // Updates the provider this proxy delegates to.
  void SetDelegate(ConfigurationPolicyProvider* delegate);

  // ConfigurationPolicyProvider:
  virtual void Shutdown() OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;

  // ConfigurationPolicyProvider::Observer:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;

 private:
  ConfigurationPolicyProvider* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyProvider);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_
