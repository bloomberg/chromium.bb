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

MockConfigurationPolicyStore::~MockConfigurationPolicyStore() {}

const Value* MockConfigurationPolicyStore::Get(
    ConfigurationPolicyType type) const {
  return policy_map_.Get(type);
}

void MockConfigurationPolicyStore::ApplyToMap(
    ConfigurationPolicyType policy, Value* value) {
  policy_map_.Set(policy, value);
}

}  // namespace policy
