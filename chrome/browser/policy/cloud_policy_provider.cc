// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_provider.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

CloudPolicyProvider::CloudPolicyProvider(
    BrowserPolicyConnector* browser_policy_connector,
    const PolicyDefinitionList* policy_list,
    PolicyLevel level)
    : ConfigurationPolicyProvider(policy_list),
      browser_policy_connector_(browser_policy_connector),
      level_(level),
      initialization_complete_(false) {
  for (size_t i = 0; i < CACHE_SIZE; ++i)
    caches_[i] = NULL;
}

CloudPolicyProvider::~CloudPolicyProvider() {
  for (size_t i = 0; i < CACHE_SIZE; ++i) {
    if (caches_[i])
      caches_[i]->RemoveObserver(this);
  }
}

void CloudPolicyProvider::SetUserPolicyCache(CloudPolicyCacheBase* cache) {
  DCHECK(!caches_[CACHE_USER]);
  caches_[CACHE_USER] = cache;
  cache->AddObserver(this);
  Merge();
}

#if defined(OS_CHROMEOS)
void CloudPolicyProvider::SetDevicePolicyCache(CloudPolicyCacheBase* cache) {
  DCHECK(caches_[CACHE_DEVICE] == NULL);
  caches_[CACHE_DEVICE] = cache;
  cache->AddObserver(this);
  Merge();
}
#endif

bool CloudPolicyProvider::IsInitializationComplete() const {
  return initialization_complete_;
}

void CloudPolicyProvider::RefreshPolicies() {
  for (size_t i = 0; i < CACHE_SIZE; ++i) {
    if (caches_[i])
      pending_updates_.insert(caches_[i]);
  }
  if (pending_updates_.empty())
    Merge();
  else
    browser_policy_connector_->FetchCloudPolicy();
}

void CloudPolicyProvider::OnCacheUpdate(CloudPolicyCacheBase* cache) {
  pending_updates_.erase(cache);
  if (pending_updates_.empty())
    Merge();
}

void CloudPolicyProvider::OnCacheGoingAway(CloudPolicyCacheBase* cache) {
  for (size_t i = 0; i < CACHE_SIZE; ++i) {
    if (caches_[i] == cache) {
      caches_[i] = NULL;
      cache->RemoveObserver(this);
      pending_updates_.erase(cache);
      Merge();
      return;
    }
  }
  NOTREACHED();
}

void CloudPolicyProvider::Merge() {
  // Re-check whether all caches are ready.
  if (!initialization_complete_) {
    initialization_complete_ = true;
    for (size_t i = 0; i < CACHE_SIZE; ++i) {
      if (caches_[i] == NULL || !caches_[i]->IsReady()) {
        initialization_complete_ = false;
        break;
      }
    }
  }

  PolicyMap combined;
  for (size_t i = 0; i < CACHE_SIZE; ++i) {
    if (caches_[i] && caches_[i]->IsReady())
      combined.MergeFrom(*caches_[i]->policy());
  }
  combined.FilterLevel(level_);

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->Get(POLICY_DOMAIN_CHROME, std::string()).Swap(&combined);
  UpdatePolicy(bundle.Pass());
}

}  // namespace policy
