// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_

#include "base/basictypes.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/schema_registry.h"
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

  // Make public for tests.
  using ConfigurationPolicyProvider::UpdatePolicy;

  // Utility method that invokes UpdatePolicy() with a PolicyBundle that maps
  // the Chrome namespace to a copy of |policy|.
  void UpdateChromePolicy(const PolicyMap& policy);

  // Convenience method so that tests don't need to create a registry to create
  // this mock.
  using ConfigurationPolicyProvider::Init;
  void Init() {
    ConfigurationPolicyProvider::Init(&registry_);
  }

  // Convenience method that installs an expectation on RefreshPolicies that
  // just notifies the observers and serves the same policies.
  void SetAutoRefresh();

 private:
  void RefreshWithSamePolicies();

  SchemaRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MockConfigurationPolicyProvider);
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
