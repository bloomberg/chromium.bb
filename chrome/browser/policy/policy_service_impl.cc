// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_impl.h"

#include "base/observer_list.h"
#include "base/stl_util.h"

namespace policy {

struct PolicyServiceImpl::Entry {
  PolicyMap policies;
  ObserverList<PolicyService::Observer, true> observers;
};

struct PolicyServiceImpl::ProviderData {
  ConfigurationPolicyProvider* provider;
  ConfigurationPolicyObserverRegistrar registrar;
  PolicyMap policies;
};

PolicyServiceImpl::PolicyServiceImpl(const Providers& providers) {
  initialization_complete_ = true;
  for (size_t i = 0; i < providers.size(); ++i) {
    ConfigurationPolicyProvider* provider = providers[i];
    ProviderData* data = new ProviderData;
    data->provider = provider;
    data->registrar.Init(provider, this);
    if (provider->IsInitializationComplete())
      provider->Provide(&data->policies);
    else
      initialization_complete_ = false;
    providers_.push_back(data);
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::~PolicyServiceImpl() {
  STLDeleteElements(&providers_);
  STLDeleteValues(&entries_);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    const std::string& component_id,
                                    PolicyService::Observer* observer) {
  PolicyNamespace ns = std::make_pair(domain, component_id);
  GetOrCreate(ns)->observers.AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       const std::string& component_id,
                                       PolicyService::Observer* observer) {
  PolicyNamespace ns = std::make_pair(domain, component_id);
  EntryMap::const_iterator it = entries_.find(ns);
  if (it == entries_.end()) {
    NOTREACHED();
    return;
  }
  it->second->observers.RemoveObserver(observer);
  MaybeCleanup(ns);
}

const PolicyMap* PolicyServiceImpl::GetPolicies(
    PolicyDomain domain,
    const std::string& component_id) const {
  PolicyNamespace ns = std::make_pair(domain, component_id);
  EntryMap::const_iterator it = entries_.find(ns);
  return it == entries_.end() ? NULL : &it->second->policies;
}

bool PolicyServiceImpl::IsInitializationComplete() const {
  return initialization_complete_;
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  ProviderList::iterator it = GetProviderData(provider);
  if (it == providers_.end())
    return;
  provider->Provide(&(*it)->policies);
  MergeAndTriggerUpdates();
}

void PolicyServiceImpl::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {
  ProviderList::iterator it = GetProviderData(provider);
  if (it == providers_.end())
    return;
  delete *it;
  providers_.erase(it);
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::Entry* PolicyServiceImpl::GetOrCreate(
    const PolicyNamespace& ns) {
  Entry*& entry = entries_[ns];
  if (!entry)
    entry = new Entry;
  return entry;
}

PolicyServiceImpl::ProviderList::iterator PolicyServiceImpl::GetProviderData(
    ConfigurationPolicyProvider* provider) {
  for (ProviderList::iterator it = providers_.begin();
       it != providers_.end(); ++it) {
    if ((*it)->provider == provider)
      return it;
  }
  NOTREACHED();
  return providers_.end();
}

void PolicyServiceImpl::MaybeCleanup(const PolicyNamespace& ns) {
  EntryMap::iterator it = entries_.find(ns);
  if (it != entries_.end() &&
      it->second->policies.empty() &&
      it->second->observers.size() == 0) {
    delete it->second;
    entries_.erase(it);
  }
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // TODO(joaodasilva): do this for each namespace once the providers also
  // provide policy for more namespaces.
  PolicyMap new_policies;
  for (ProviderList::iterator it = providers_.begin();
       it != providers_.end(); ++it) {
    new_policies.MergeFrom((*it)->policies);
  }

  Entry* entry = GetOrCreate(std::make_pair(POLICY_DOMAIN_CHROME, ""));
  if (!new_policies.Equals(entry->policies)) {
    entry->policies.Swap(&new_policies);
    FOR_EACH_OBSERVER(PolicyService::Observer,
                      entry->observers,
                      OnPolicyUpdated(POLICY_DOMAIN_CHROME, ""));
  }

  // Check if all providers became initialized just now, if they weren't before.
  if (!initialization_complete_) {
    initialization_complete_ = true;
    for (ProviderList::iterator iter = providers_.begin();
         iter != providers_.end(); ++iter) {
      if (!(*iter)->provider->IsInitializationComplete()) {
        initialization_complete_ = false;
        break;
      }
    }
    if (initialization_complete_) {
      for (EntryMap::iterator i = entries_.begin(); i != entries_.end(); ++i) {
        FOR_EACH_OBSERVER(PolicyService::Observer,
                          i->second->observers,
                          OnPolicyServiceInitialized());
      }
    }
  }
}

}  // namespace policy
