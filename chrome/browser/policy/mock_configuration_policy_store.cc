// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_configuration_policy_store.h"

namespace policy {

using ::testing::_;
using ::testing::Invoke;

MockConfigurationPolicyStore::MockConfigurationPolicyStore() {
  ON_CALL(*this, Apply(_, _)).WillByDefault(
      Invoke(this, &MockConfigurationPolicyStore::ApplyToMap));
}

MockConfigurationPolicyStore::~MockConfigurationPolicyStore() {
  STLDeleteValues(&policy_map_);
}

const Value* MockConfigurationPolicyStore::Get(
    ConfigurationPolicyType type) const {
  PolicyMap::const_iterator entry(policy_map_.find(type));
  return entry == policy_map_.end() ? NULL : entry->second;
}

void MockConfigurationPolicyStore::ApplyToMap(
    ConfigurationPolicyType policy, Value* value) {
  std::swap(policy_map_[policy], value);
  delete value;
}

}  // namespace policy
