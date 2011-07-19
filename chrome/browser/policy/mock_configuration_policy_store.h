// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
#pragma once

#include <map>
#include <utility>

#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/browser/policy/policy_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock ConfigurationPolicyStore implementation that records values for policy
// settings as they get set.
class MockConfigurationPolicyStore : public ConfigurationPolicyStoreInterface {
 public:
  MockConfigurationPolicyStore();
  virtual ~MockConfigurationPolicyStore();

  const PolicyMap& policy_map() const { return policy_map_; }

  // Get a value for the given policy. Returns NULL if that key doesn't exist.
  const Value* Get(ConfigurationPolicyType type) const;
  // ConfigurationPolicyStore implementation.
  void ApplyToMap(ConfigurationPolicyType policy, Value* value);

  MOCK_METHOD2(Apply, void(ConfigurationPolicyType policy, Value* value));

 private:
  PolicyMap policy_map_;

  DISALLOW_COPY_AND_ASSIGN(MockConfigurationPolicyStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_STORE_H_
