// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dummy_cloud_policy_provider.h"

namespace policy {

DummyCloudPolicyProvider::DummyCloudPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : CloudPolicyProvider(policy_list) {
}

DummyCloudPolicyProvider::~DummyCloudPolicyProvider() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnProviderGoingAway());
}

void DummyCloudPolicyProvider::AppendCache(CloudPolicyCacheBase* cache) {
}

void DummyCloudPolicyProvider::PrependCache(CloudPolicyCacheBase* cache) {
}

bool DummyCloudPolicyProvider::Provide(PolicyMap* map) {
  return true;
}
void DummyCloudPolicyProvider::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DummyCloudPolicyProvider::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace policy
