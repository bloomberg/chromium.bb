// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "policy/policy_constants.h"

namespace policy {

typedef PolicyServiceImpl::Providers::const_iterator Iterator;

namespace {

const char* kProxyPolicies[] = {
  key::kProxyMode,
  key::kProxyServerMode,
  key::kProxyServer,
  key::kProxyPacUrl,
  key::kProxyBypassList,
};

void FixDeprecatedPolicies(PolicyMap* policies) {
  // Proxy settings have been configured by 5 policies that didn't mix well
  // together, and maps of policies had to take this into account when merging
  // policy sources. The proxy settings will eventually be configured by a
  // single Dictionary policy when all providers have support for that. For
  // now, the individual policies are mapped here to a single Dictionary policy
  // that the rest of the policy machinery uses.

  // The highest (level, scope) pair for an existing proxy policy is determined
  // first, and then only policies with those exact attributes are merged.
  PolicyMap::Entry current_priority;  // Defaults to the lowest priority.
  PolicySource inherited_source = POLICY_SOURCE_ENTERPRISE_DEFAULT;
  std::unique_ptr<base::DictionaryValue> proxy_settings(
      new base::DictionaryValue);
  for (size_t i = 0; i < arraysize(kProxyPolicies); ++i) {
    const PolicyMap::Entry* entry = policies->Get(kProxyPolicies[i]);
    if (entry) {
      if (entry->has_higher_priority_than(current_priority)) {
        proxy_settings->Clear();
        current_priority = entry->DeepCopy();
        if (entry->source > inherited_source)  // Higher priority?
          inherited_source = entry->source;
      }
      if (!entry->has_higher_priority_than(current_priority) &&
          !current_priority.has_higher_priority_than(*entry)) {
        proxy_settings->Set(kProxyPolicies[i], entry->value->CreateDeepCopy());
      }
      policies->Erase(kProxyPolicies[i]);
    }
  }
  // Sets the new |proxy_settings| if kProxySettings isn't set yet, or if the
  // new priority is higher.
  const PolicyMap::Entry* existing = policies->Get(key::kProxySettings);
  if (!proxy_settings->empty() &&
      (!existing || current_priority.has_higher_priority_than(*existing))) {
    policies->Set(key::kProxySettings, current_priority.level,
                  current_priority.scope, inherited_source,
                  std::move(proxy_settings), nullptr);
  }
}

}  // namespace

PolicyServiceImpl::PolicyServiceImpl(const Providers& providers)
    : update_task_ptr_factory_(this) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    initialization_complete_[domain] = true;
  providers_ = providers;
  for (Iterator it = providers.begin(); it != providers.end(); ++it) {
    ConfigurationPolicyProvider* provider = *it;
    provider->AddObserver(this);
    for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
      initialization_complete_[domain] &=
          provider->IsInitializationComplete(static_cast<PolicyDomain>(domain));
    }
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::~PolicyServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
    (*it)->RemoveObserver(this);
  STLDeleteValues(&observers_);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Observers*& list = observers_[domain];
  if (!list)
    list = new Observers();
  list->AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ObserverMap::iterator it = observers_.find(domain);
  if (it == observers_.end()) {
    NOTREACHED();
    return;
  }
  it->second->RemoveObserver(observer);
  if (!it->second->might_have_observers()) {
    delete it->second;
    observers_.erase(it);
  }
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    const PolicyNamespace& ns) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return policy_bundle_.Get(ns);
}

bool PolicyServiceImpl::IsInitializationComplete(PolicyDomain domain) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return initialization_complete_[domain];
}

void PolicyServiceImpl::RefreshPolicies(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!callback.is_null())
    refresh_callbacks_.push_back(callback);

  if (providers_.empty()) {
    // Refresh is immediately complete if there are no providers. See the note
    // on OnUpdatePolicy() about why this is a posted task.
    update_task_ptr_factory_.InvalidateWeakPtrs();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PolicyServiceImpl::MergeAndTriggerUpdates,
                              update_task_ptr_factory_.GetWeakPtr()));
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
      refresh_pending_.insert(*it);
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
      (*it)->RefreshPolicies();
  }
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(1, std::count(providers_.begin(), providers_.end(), provider));
  refresh_pending_.erase(provider);

  // Note: a policy change may trigger further policy changes in some providers.
  // For example, disabling SigninAllowed would cause the CloudPolicyManager to
  // drop all its policies, which makes this method enter again for that
  // provider.
  //
  // Therefore this update is posted asynchronously, to prevent reentrancy in
  // MergeAndTriggerUpdates. Also, cancel a pending update if there is any,
  // since both will produce the same PolicyBundle.
  update_task_ptr_factory_.InvalidateWeakPtrs();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PolicyServiceImpl::MergeAndTriggerUpdates,
                            update_task_ptr_factory_.GetWeakPtr()));
}

void PolicyServiceImpl::NotifyNamespaceUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ObserverMap::iterator iterator = observers_.find(ns.domain);
  if (iterator != observers_.end()) {
    FOR_EACH_OBSERVER(PolicyService::Observer,
                      *iterator->second,
                      OnPolicyUpdated(ns, previous, current));
  }
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // Merge from each provider in their order of priority.
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());
  PolicyBundle bundle;
  for (Iterator it = providers_.begin(); it != providers_.end(); ++it) {
    PolicyBundle provided_bundle;
    provided_bundle.CopyFrom((*it)->policies());
    FixDeprecatedPolicies(&provided_bundle.Get(chrome_namespace));
    bundle.MergeFrom(provided_bundle);
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
    } else if (it_old->first < it_new->first) {
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
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if all the providers just became initialized for each domain; if so,
  // notify that domain's observers.
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
    if (initialization_complete_[domain])
      continue;

    PolicyDomain policy_domain = static_cast<PolicyDomain>(domain);

    bool all_complete = true;
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it) {
      if (!(*it)->IsInitializationComplete(policy_domain)) {
        all_complete = false;
        break;
      }
    }
    if (all_complete) {
      initialization_complete_[domain] = true;
      ObserverMap::iterator iter = observers_.find(policy_domain);
      if (iter != observers_.end()) {
        FOR_EACH_OBSERVER(PolicyService::Observer,
                          *iter->second,
                          OnPolicyServiceInitialized(policy_domain));
      }
    }
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::Closure> callbacks;
    callbacks.swap(refresh_callbacks_);
    std::vector<base::Closure>::iterator it;
    for (it = callbacks.begin(); it != callbacks.end(); ++it)
      it->Run();
  }
}

}  // namespace policy
