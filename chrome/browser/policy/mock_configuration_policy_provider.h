// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include <map>
#include <utility>

#include "base/stl_util-inl.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  MockConfigurationPolicyProvider();
  virtual ~MockConfigurationPolicyProvider();

  void AddPolicy(ConfigurationPolicyType policy, Value* value);

  // ConfigurationPolicyProvider method overrides.
  virtual bool Provide(ConfigurationPolicyStoreInterface* store);

 private:
  typedef std::map<ConfigurationPolicyType, Value*> PolicyMap;

  PolicyMap policy_map_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
