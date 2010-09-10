// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#pragma once

#include <map>

#include "base/stl_util-inl.h"
#include "chrome/browser/policy/configuration_policy_store.h"

// Mock ConfigurationPolicyStore implementation that records values for policy
// settings as they get set.
class MockConfigurationPolicyStore : public ConfigurationPolicyStore {
 public:
  MockConfigurationPolicyStore() {}
  ~MockConfigurationPolicyStore() {
    STLDeleteValues(&policy_map_);
  }

  typedef std::map<ConfigurationPolicyStore::PolicyType, Value*> PolicyMap;
  const PolicyMap& policy_map() { return policy_map_; }

  // ConfigurationPolicyStore implementation.
  virtual void Apply(PolicyType policy, Value* value) {
    policy_map_[policy] = value;
  }

 private:
  PolicyMap policy_map_;
};

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
