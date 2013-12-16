// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyMap;

class POLICY_EXPORT PolicyServiceImpl
    : public PolicyService,
      public ConfigurationPolicyProvider::Observer {
 public:
  typedef std::vector<ConfigurationPolicyProvider*> Providers;

  // The PolicyServiceImpl will merge policies from |providers|. |providers|
  // must be sorted in decreasing order of priority; the first provider will
  // have the highest priority. The PolicyServiceImpl does not take ownership of
  // the providers, and they must outlive the PolicyServiceImpl.
  explicit PolicyServiceImpl(const Providers& providers);

  virtual ~PolicyServiceImpl();

  // PolicyService overrides:
  virtual void AddObserver(PolicyDomain domain,
                           PolicyService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PolicyDomain domain,
                              PolicyService::Observer* observer) OVERRIDE;
  virtual const PolicyMap& GetPolicies(
      const PolicyNamespace& ns) const OVERRIDE;
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;
  virtual void RefreshPolicies(const base::Closure& callback) OVERRIDE;

 private:
  typedef ObserverList<PolicyService::Observer, true> Observers;
  typedef std::map<PolicyDomain, Observers*> ObserverMap;

  // ConfigurationPolicyProvider::Observer overrides:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;

  // Posts a task to notify observers of |ns| that its policies have changed,
  // passing along the |previous| and the |current| policies.
  void NotifyNamespaceUpdated(const PolicyNamespace& ns,
                              const PolicyMap& previous,
                              const PolicyMap& current);

  // Combines the policies from all the providers, and notifies the observers
  // of namespaces whose policies have been modified.
  void MergeAndTriggerUpdates();

  // Checks if all providers are initialized, and notifies the observers
  // if the service just became initialized.
  void CheckInitializationComplete();

  // Invokes all the refresh callbacks if there are no more refreshes pending.
  void CheckRefreshComplete();

  // The providers passed in the constructor, in order of decreasing priority.
  Providers providers_;

  // Maps each policy namespace to its current policies.
  PolicyBundle policy_bundle_;

  // Maps each policy domain to its observer list.
  ObserverMap observers_;

  // True if all the providers are initialized for the indexed policy domain.
  bool initialization_complete_[POLICY_DOMAIN_SIZE];

  // Set of providers that have a pending update that was triggered by a
  // call to RefreshPolicies().
  std::set<ConfigurationPolicyProvider*> refresh_pending_;

  // List of callbacks to invoke once all providers refresh after a
  // RefreshPolicies() call.
  std::vector<base::Closure> refresh_callbacks_;

  // Used to create tasks to delay new policy updates while we may be already
  // processing previous policy updates.
  base::WeakPtrFactory<PolicyServiceImpl> update_task_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceImpl);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_
