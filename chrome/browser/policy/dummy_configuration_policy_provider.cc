// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dummy_configuration_policy_provider.h"

namespace policy {

DummyConfigurationPolicyProvider::DummyConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : ConfigurationPolicyProvider(policy_list) {
}

DummyConfigurationPolicyProvider::~DummyConfigurationPolicyProvider() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnProviderGoingAway());
}

bool DummyConfigurationPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  return true;
}

void DummyConfigurationPolicyProvider::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DummyConfigurationPolicyProvider::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace policy
