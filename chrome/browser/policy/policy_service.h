// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

// Policies are namespaced by a (PolicyDomain, ID) pair. The meaning of the ID
// string depends on the domain; for example, if the PolicyDomain is
// "extensions" then the ID identifies the extension that the policies control.
enum PolicyDomain {
  // The component ID for chrome policies is always the empty string.
  POLICY_DOMAIN_CHROME,

  // The extensions policy domain is a work in progress. Included here for
  // tests.
  POLICY_DOMAIN_EXTENSIONS,
};

// The PolicyService merges policies from all available sources, taking into
// account their priorities. Policy clients can retrieve policy for their domain
// and register for notifications on policy updates.
//
// The PolicyService is available from BrowserProcess as a global singleton.
class PolicyService {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Invoked whenever policies for the |domain|, |component_id| namespace are
    // modified. This is only invoked for changes that happen after AddObserver
    // is called. |previous| contains the values of the policies before the
    // update, and |current| contains the current values.
    virtual void OnPolicyUpdated(PolicyDomain domain,
                                 const std::string& component_id,
                                 const PolicyMap& previous,
                                 const PolicyMap& current) = 0;

    // Invoked at most once, when the PolicyService becomes ready. If
    // IsInitializationComplete() is false, then this will be invoked once all
    // the policy providers are ready.
    virtual void OnPolicyServiceInitialized() {}
  };

  virtual ~PolicyService() {}

  virtual void AddObserver(PolicyDomain domain,
                           const std::string& component_id,
                           Observer* observer) = 0;

  virtual void RemoveObserver(PolicyDomain domain,
                              const std::string& component_id,
                              Observer* observer) = 0;

  // Returns NULL if no policies are available for |domain|, | component_id|.
  virtual const PolicyMap* GetPolicies(
      PolicyDomain domain,
      const std::string& component_id) const = 0;

  // The PolicyService loads policy from several sources, and some require
  // asynchronous loads. IsInitializationComplete() returns true once all
  // sources have loaded their policies. It is safe to read policy from the
  // PolicyService even if IsInitializationComplete() is false; there will be an
  // OnPolicyUpdated() notification once new policies become available.
  //
  // OnPolicyServiceInitialized() is called when IsInitializationComplete()
  // becomes true, which happens at most once. If IsInitializationComplete() is
  // already true when an Observer is registered, then that Observer will not
  // have a OnPolicyServiceInitialized() notification.
  virtual bool IsInitializationComplete() const = 0;

  // Asks the PolicyService to reload policy from all available policy sources.
  // |callback| is invoked once every source has reloaded its policies, and
  // GetPolicies() is guaranteed to return the updated values at that point.
  virtual void RefreshPolicies(const base::Closure& callback) = 0;
};

// A registrar that only observes changes to particular policies within the
// PolicyMap for the given policy namespace.
class PolicyChangeRegistrar : public PolicyService::Observer {
 public:
  typedef base::Callback<void(const Value*, const Value*)> UpdateCallback;

  // Observes updates to the given (domain, component_id) namespace in the given
  // |policy_service|, and notifies |observer| whenever any of the registered
  // policy keys changes. Both the |policy_service| and the |observer| must
  // outlive |this|.
  PolicyChangeRegistrar(PolicyService* policy_service,
                        PolicyDomain domain,
                        const std::string& component_id);

  virtual ~PolicyChangeRegistrar();

  // Will invoke |callback| whenever |policy_name| changes its value, as long
  // as this registrar exists.
  // Only one callback can be registed per policy name; a second call with the
  // same |policy_name| will overwrite the previous callback.
  void Observe(const std::string& policy_name, const UpdateCallback& callback);

  // Implementation of PolicyService::Observer:
  virtual void OnPolicyUpdated(PolicyDomain domain,
                               const std::string& component_id,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;

 private:
  typedef std::map<std::string, UpdateCallback> CallbackMap;

  PolicyService* policy_service_;
  PolicyDomain domain_;
  std::string component_id_;
  CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyChangeRegistrar);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
