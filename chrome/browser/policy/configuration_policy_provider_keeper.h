// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_KEEPER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_KEEPER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

// Manages the lifecycle of the shared platform-specific policy providers for
// managed platform, device management and recommended policy.
class ConfigurationPolicyProviderKeeper {
 public:
  ConfigurationPolicyProviderKeeper();
  // Tests can pass in their own (dummy) providers using this c'tor.
  ConfigurationPolicyProviderKeeper(
      ConfigurationPolicyProvider* managed_platform_provider,
      ConfigurationPolicyProvider* device_management_provider,
      ConfigurationPolicyProvider* recommended_provider);
  virtual ~ConfigurationPolicyProviderKeeper();

  ConfigurationPolicyProvider* managed_platform_provider() const;

  ConfigurationPolicyProvider* device_management_provider() const;

  ConfigurationPolicyProvider* recommended_provider() const;

 private:
  scoped_ptr<ConfigurationPolicyProvider> managed_platform_provider_;
  scoped_ptr<ConfigurationPolicyProvider> device_management_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_provider_;

  static ConfigurationPolicyProvider* CreateManagedPlatformProvider();
  static ConfigurationPolicyProvider* CreateDeviceManagementProvider();
  static ConfigurationPolicyProvider* CreateRecommendedProvider();

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderKeeper);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_KEEPER_H_
