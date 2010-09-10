// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include <map>

#include "base/stl_util-inl.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  MockConfigurationPolicyProvider() {}
  ~MockConfigurationPolicyProvider() {
    STLDeleteValues(&policy_map_);
  }

  typedef std::map<ConfigurationPolicyStore::PolicyType, Value*> PolicyMap;

  void AddPolicy(ConfigurationPolicyStore::PolicyType policy, Value* value) {
    policy_map_[policy] = value;
  }

  // ConfigurationPolicyProvider method overrides.
  virtual bool Provide(ConfigurationPolicyStore* store) {
    for (PolicyMap::const_iterator current = policy_map_.begin();
         current != policy_map_.end(); ++current) {
      store->Apply(current->first, current->second->DeepCopy());
    }
    return true;
  }

 private:
  PolicyMap policy_map_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
