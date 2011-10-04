// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_provider_impl.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"

namespace policy {

CloudPolicyProviderImpl::CloudPolicyProviderImpl(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    CloudPolicyCacheBase::PolicyLevel level)
    : CloudPolicyProvider(policy_list),
      level_(level),
      initialization_complete_(true) {}

CloudPolicyProviderImpl::~CloudPolicyProviderImpl() {
  for (ListType::iterator i = caches_.begin(); i != caches_.end(); ++i)
    (*i)->RemoveObserver(this);
}

bool CloudPolicyProviderImpl::ProvideInternal(PolicyMap* result) {
  result->CopyFrom(combined_);
  return true;
}

bool CloudPolicyProviderImpl::IsInitializationComplete() const {
  return initialization_complete_;
}

void CloudPolicyProviderImpl::OnCacheUpdate(CloudPolicyCacheBase* cache) {
  RecombineCachesAndTriggerUpdate();
}

void CloudPolicyProviderImpl::OnCacheGoingAway(CloudPolicyCacheBase* cache) {
  cache->RemoveObserver(this);
  for (ListType::iterator i = caches_.begin(); i != caches_.end(); ++i) {
    if (*i == cache) {
      caches_.erase(i);
      break;
    }
  }

  RecombineCachesAndTriggerUpdate();
}

void CloudPolicyProviderImpl::AppendCache(CloudPolicyCacheBase* cache) {
  initialization_complete_ &= cache->IsReady();
  cache->AddObserver(this);
  caches_.push_back(cache);
  RecombineCachesAndTriggerUpdate();
}

void CloudPolicyProviderImpl::PrependCache(CloudPolicyCacheBase* cache) {
  initialization_complete_ &= cache->IsReady();
  cache->AddObserver(this);
  caches_.insert(caches_.begin(), cache);
  RecombineCachesAndTriggerUpdate();
}

// static
void CloudPolicyProviderImpl::CombineTwoPolicyMaps(const PolicyMap& base,
                                                   const PolicyMap& overlay,
                                                   PolicyMap* out_map) {
  bool added_proxy_policy = false;
  out_map->Clear();

  for (PolicyMap::const_iterator i = base.begin(); i != base.end(); ++i) {
    out_map->Set(i->first, i->second->DeepCopy());
    added_proxy_policy = added_proxy_policy ||
                         ConfigurationPolicyPrefStore::IsProxyPolicy(i->first);
  }

  // Add every entry of |overlay| which has not been added by |base|. Only add
  // proxy policies if none of them was added by |base|.
  for (PolicyMap::const_iterator i = overlay.begin(); i != overlay.end(); ++i) {
    if (ConfigurationPolicyPrefStore::IsProxyPolicy(i->first)) {
      if (!added_proxy_policy) {
        out_map->Set(i->first, i->second->DeepCopy());
      }
    } else if (!out_map->Get(i->first)) {
      out_map->Set(i->first, i->second->DeepCopy());
    }
  }
}

void CloudPolicyProviderImpl::RecombineCachesAndTriggerUpdate() {
  // Re-check whether all caches are ready.
  if (!initialization_complete_) {
    bool all_caches_ready = true;
    for (ListType::const_iterator i = caches_.begin();
         i != caches_.end(); ++i) {
      if (!(*i)->IsReady()) {
        all_caches_ready = false;
        break;
      }
    }
    if (all_caches_ready)
      initialization_complete_ = true;
  }

  // Reconstruct the merged policy map.
  PolicyMap newly_combined;
  for (ListType::iterator i = caches_.begin(); i != caches_.end(); ++i) {
    if (!(*i)->IsReady())
      continue;
    PolicyMap tmp_map;
    CombineTwoPolicyMaps(newly_combined, *(*i)->policy(level_), &tmp_map);
    newly_combined.Swap(&tmp_map);
  }

  // Trigger a notification.
  combined_.Swap(&newly_combined);
  NotifyPolicyUpdated();
}

}  // namespace policy
