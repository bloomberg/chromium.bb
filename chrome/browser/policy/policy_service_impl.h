// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

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
  virtual const PolicyMap* GetPolicies(
      PolicyDomain domain,
      const std::string& component_id) const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual void RefreshPolicies(const base::Closure& callback) OVERRIDE;

 private:
  struct Entry;
  struct ProviderData;

  typedef std::pair<PolicyDomain, std::string> PolicyNamespace;
  typedef std::map<PolicyNamespace, Entry*> EntryMap;
  typedef std::vector<ProviderData*> ProviderList;

  // ConfigurationPolicyProvider::Observer overrides:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;
  virtual void OnProviderGoingAway(
      ConfigurationPolicyProvider* provider) OVERRIDE;

  Entry* GetOrCreate(const PolicyNamespace& ns);
  ProviderList::iterator GetProviderData(ConfigurationPolicyProvider* provider);

  // Erases the entry for |ns|, if it has no observers and no policies.
  void MaybeCleanup(const PolicyNamespace& ns);

  // Combines the policies from all the providers, and notifies the observers
  // of namespaces whose policies have been modified.
  // |is_refresh| should be true if MergeAndTriggerUpdates() was invoked because
  // a provider has just refreshed its policies.
  void MergeAndTriggerUpdates(bool is_refresh);

  // Contains all the providers together with a cached copy of their policies
  // and their registrars.
  ProviderList providers_;

  // Maps each policy namespace to its current policies.
  EntryMap entries_;

  // True if all the providers are initialized.
  bool initialization_complete_;

  // List of callbacks to invoke once all providers refresh after a
  // RefreshPolicies() call.
  std::vector<base::Closure> refresh_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_IMPL_H_
