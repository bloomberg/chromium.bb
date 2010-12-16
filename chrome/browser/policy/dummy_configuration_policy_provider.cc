// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dummy_configuration_policy_provider.h"

namespace policy {

DummyConfigurationPolicyProvider::DummyConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : ConfigurationPolicyProvider(policy_list) {
}

DummyConfigurationPolicyProvider::~DummyConfigurationPolicyProvider() {
}

bool DummyConfigurationPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  return true;
}

}  // namespace policy
