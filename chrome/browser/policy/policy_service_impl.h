// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

class PolicyMap;

class PolicyServiceImpl : public PolicyService,
                          public ConfigurationPolicyProvider::Observer {
 public:
  typedef std::vector<ConfigurationPolicyProvider*> Providers;

  // The PolicyServiceImpl will merge policies from |providers|. |providers|
  // must be sorted in decreasing order of priority; the first provider will
  // have the highest priority. The PolicyServiceImpl does not take ownership of
  // the providers, but handles OnProviderGoingAway() if they are destroyed.
  explicit PolicyServiceImpl(const Providers& providers);
  virtual ~PolicyServiceImpl();

  // PolicyService overrides:
  virtual void AddObserver(PolicyDomain domain,
                           const std::string& component_id,
                           PolicyService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PolicyDomain domain,
                              const std::string& component_id,
                              PolicyService::Observer* observer) OVERRIDE;
  virtual const PolicyMap& GetPolicies(
      PolicyDomain domain,
      const std::string& component_id) const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies(const base::Closure& callback) OVERRIDE;

 private:
  typedef ObserverList<PolicyService::Observer, true> Observers;
  typedef std::map<PolicyBundle::PolicyNamespace, Observers*> ObserverMap;
  typedef std::vector<ConfigurationPolicyObserverRegistrar*> RegistrarList;

  // ConfigurationPolicyProvider::Observer overrides:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;
  virtual void OnProviderGoingAway(
      ConfigurationPolicyProvider* provider) OVERRIDE;

  // Returns an iterator to the entry in |registrars_| that corresponds to
  // |provider|, or |registrars_.end()|.
  RegistrarList::iterator GetRegistrar(ConfigurationPolicyProvider* provider);

  // Notifies observers of |ns| that its policies have changed, passing along
  // the |previous| and the |current| policies.
  void NotifyNamespaceUpdated(const PolicyBundle::PolicyNamespace& ns,
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

  // Contains a registrar for each of the providers passed in the constructor,
  // in order of decreasing priority.
  RegistrarList registrars_;

  // Maps each policy namespace to its current policies.
  PolicyBundle policy_bundle_;

  // Maps each policy namespace to its observer list.
  ObserverMap observers_;

  // True if all the providers are initialized.
  bool initialization_complete_;

  // Set of providers that have a pending update that was triggered by a
  // call to RefreshPolicies().
  std::set<ConfigurationPolicyProvider*> refresh_pending_;

  // List of callbacks to invoke once all providers refresh after a
  // RefreshPolicies() call.
  std::vector<base::Closure> refresh_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
