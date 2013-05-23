// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_

#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_domain_descriptor.h"
#include "chrome/browser/policy/policy_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
// TODO(joaodasilva, mnissler): introduce an implementation that non-policy
// code can use that doesn't require the usual boilerplate.
// http://crbug.com/242087
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  MockConfigurationPolicyProvider();
  virtual ~MockConfigurationPolicyProvider();

  MOCK_CONST_METHOD1(IsInitializationComplete, bool(PolicyDomain domain));
  MOCK_METHOD0(RefreshPolicies, void());

  MOCK_METHOD1(RegisterPolicyDomain,
               void(scoped_refptr<const PolicyDomainDescriptor>));

  // Make public for tests.
  using ConfigurationPolicyProvider::UpdatePolicy;

  // Utility method that invokes UpdatePolicy() with a PolicyBundle that maps
  // the Chrome namespace to a copy of |policy|.
  void UpdateChromePolicy(const PolicyMap& policy);
};

class MockConfigurationPolicyObserver
    : public ConfigurationPolicyProvider::Observer {
 public:
  MockConfigurationPolicyObserver();
  virtual ~MockConfigurationPolicyObserver();

  MOCK_METHOD1(OnUpdatePolicy, void(ConfigurationPolicyProvider*));
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
