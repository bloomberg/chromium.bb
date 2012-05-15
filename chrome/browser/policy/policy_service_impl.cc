// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_impl.h"

#include "base/stl_util.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

PolicyServiceImpl::PolicyServiceImpl(const Providers& providers) {
  initialization_complete_ = true;
  for (size_t i = 0; i < providers.size(); ++i) {
    ConfigurationPolicyProvider* provider = providers[i];
    ConfigurationPolicyObserverRegistrar* registrar =
        new ConfigurationPolicyObserverRegistrar();
    registrar->Init(provider, this);
    initialization_complete_ &= provider->IsInitializationComplete();
    registrars_.push_back(registrar);
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::~PolicyServiceImpl() {
  STLDeleteElements(&registrars_);
  STLDeleteValues(&observers_);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    const std::string& component_id,
                                    PolicyService::Observer* observer) {
  PolicyBundle::PolicyNamespace ns(domain, component_id);
  Observers*& list = observers_[ns];
  if (!list)
    list = new Observers();
  list->AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       const std::string& component_id,
                                       PolicyService::Observer* observer) {
  PolicyBundle::PolicyNamespace ns(domain, component_id);
  ObserverMap::iterator it = observers_.find(ns);
  if (it == observers_.end()) {
    NOTREACHED();
    return;
  }
  it->second->RemoveObserver(observer);
  if (it->second->size() == 0) {
    delete it->second;
    observers_.erase(it);
  }
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    PolicyDomain domain,
    const std::string& component_id) const {
  return policy_bundle_.Get(domain, component_id);
}

bool PolicyServiceImpl::IsInitializationComplete() const {
  return initialization_complete_;
}

void PolicyServiceImpl::RefreshPolicies(const base::Closure& callback) {
  if (!callback.is_null())
    refresh_callbacks_.push_back(callback);

  if (registrars_.empty()) {
    // Refresh is immediately complete if there are no providers.
    MergeAndTriggerUpdates();
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    RegistrarList::iterator it;
    for (it = registrars_.begin(); it != registrars_.end(); ++it)
      refresh_pending_.insert((*it)->provider());
    for (it = registrars_.begin(); it != registrars_.end(); ++it)
      (*it)->provider()->RefreshPolicies();
  }
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  RegistrarList::iterator it = GetRegistrar(provider);
  if (it == registrars_.end())
    return;
  refresh_pending_.erase(provider);
  MergeAndTriggerUpdates();
}

void PolicyServiceImpl::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {
  RegistrarList::iterator it = GetRegistrar(provider);
  if (it == registrars_.end())
    return;
  refresh_pending_.erase(provider);
  delete *it;
  registrars_.erase(it);
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::RegistrarList::iterator PolicyServiceImpl::GetRegistrar(
    ConfigurationPolicyProvider* provider) {
  for (RegistrarList::iterator it = registrars_.begin();
       it != registrars_.end(); ++it) {
    if ((*it)->provider() == provider)
      return it;
  }
  NOTREACHED();
  return registrars_.end();
}

void PolicyServiceImpl::NotifyNamespaceUpdated(
    const PolicyBundle::PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  ObserverMap::iterator iterator = observers_.find(ns);
  if (iterator != observers_.end()) {
    FOR_EACH_OBSERVER(
        PolicyService::Observer,
        *iterator->second,
        OnPolicyUpdated(ns.first, ns.second, previous, current));
  }
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // Merge from each provider in their order of priority.
  PolicyBundle bundle;
  for (RegistrarList::iterator it = registrars_.begin();
       it != registrars_.end(); ++it) {
    bundle.MergeFrom((*it)->provider()->policies());
  }

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  policy_bundle_.Swap(&bundle);

  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = bundle.begin();
  PolicyBundle::const_iterator end_old = bundle.end();
  while (it_new != end_new && it_old != end_old) {
    if (it_new->first < it_old->first) {
      // A new namespace is available.
      NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);
      ++it_new;
    } else if (it_new->first > it_old->first) {
      // A previously available namespace is now gone.
      NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);
      ++it_old;
    } else {
      if (!it_new->second->Equals(*it_old->second)) {
        // An existing namespace's policies have changed.
        NotifyNamespaceUpdated(it_new->first, *it_old->second, *it_new->second);
      }
      ++it_new;
      ++it_old;
    }
  }

  // Send updates for the remaining new namespaces, if any.
  for (; it_new != end_new; ++it_new)
    NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old)
    NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);

  CheckInitializationComplete();
  CheckRefreshComplete();
}

void PolicyServiceImpl::CheckInitializationComplete() {
  // Check if all providers became initialized just now, if they weren't before.
  if (!initialization_complete_) {
    initialization_complete_ = true;
    for (RegistrarList::iterator iter = registrars_.begin();
         iter != registrars_.end(); ++iter) {
      if (!(*iter)->provider()->IsInitializationComplete()) {
        initialization_complete_ = false;
        break;
      }
    }
    if (initialization_complete_) {
      for (ObserverMap::iterator iter = observers_.begin();
           iter != observers_.end(); ++iter) {
        FOR_EACH_OBSERVER(PolicyService::Observer,
                          *iter->second,
                          OnPolicyServiceInitialized());
      }
    }
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::Closure> callbacks;
    callbacks.swap(refresh_callbacks_);
    for (size_t i = 0; i < callbacks.size(); ++i)
      callbacks[i].Run();
  }
}

}  // namespace policy
