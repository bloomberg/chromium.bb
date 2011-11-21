// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_configuration_policy_provider.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "policy/policy_constants.h"

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider()
    : ConfigurationPolicyProvider(GetChromePolicyDefinitionList()),
      initialization_complete_(false) {
}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {}

void MockConfigurationPolicyProvider::AddPolicy(ConfigurationPolicyType policy,
                                                Value* value) {
  policy_map_.Set(policy, value);
}

void MockConfigurationPolicyProvider::RemovePolicy(
    ConfigurationPolicyType policy) {
  policy_map_.Erase(policy);
}

void MockConfigurationPolicyProvider::SetInitializationComplete(
    bool initialization_complete) {
  initialization_complete_ = initialization_complete;
}

bool MockConfigurationPolicyProvider::ProvideInternal(PolicyMap* policies) {
  policies->CopyFrom(policy_map_);
  return true;
}

bool MockConfigurationPolicyProvider::IsInitializationComplete() const {
  return initialization_complete_;
}

void MockConfigurationPolicyProvider::RefreshPolicies() {
  NotifyPolicyUpdated();
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy
