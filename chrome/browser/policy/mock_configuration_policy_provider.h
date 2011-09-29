// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  MockConfigurationPolicyProvider();
  virtual ~MockConfigurationPolicyProvider();

  void AddPolicy(ConfigurationPolicyType policy, Value* value);
  void RemovePolicy(ConfigurationPolicyType policy);

  void SetInitializationComplete(bool initialization_complete);

  // ConfigurationPolicyProvider method overrides.
  virtual bool Provide(PolicyMap* policies) OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

 private:

  PolicyMap policy_map_;
  bool initialization_complete_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
