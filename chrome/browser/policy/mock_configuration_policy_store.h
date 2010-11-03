// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#pragma once

#include <map>
#include <utility>

#include "base/stl_util-inl.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"

namespace policy {

// Mock ConfigurationPolicyStore implementation that records values for policy
// settings as they get set.
class MockConfigurationPolicyStore : public ConfigurationPolicyStoreInterface {
 public:
  typedef std::map<ConfigurationPolicyType, Value*> PolicyMap;

  MockConfigurationPolicyStore() {}
  ~MockConfigurationPolicyStore() {
    STLDeleteValues(&policy_map_);
  }

  const PolicyMap& policy_map() const { return policy_map_; }

  // Get a value for the given policy. Returns NULL if that key doesn't exist.
  const Value* Get(ConfigurationPolicyType type) const {
    PolicyMap::const_iterator entry(policy_map_.find(type));
    return entry == policy_map_.end() ? NULL : entry->second;
  }

  // ConfigurationPolicyStore implementation.
  virtual void Apply(ConfigurationPolicyType policy, Value* value) {
    std::swap(policy_map_[policy], value);
    delete value;
  }

 private:
  PolicyMap policy_map_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
