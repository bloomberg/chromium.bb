// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <utility>

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

  // Must be the last entry.
  POLICY_DOMAIN_SIZE,
};

// Groups a policy domain and a component ID in a single object representing
// a policy namespace. Objects of this class can be used as keys in std::maps.
struct PolicyNamespace {
 public:
  PolicyNamespace();
  PolicyNamespace(PolicyDomain domain, const std::string& component_id);
  PolicyNamespace(const PolicyNamespace& other);
  ~PolicyNamespace();

  PolicyNamespace& operator=(const PolicyNamespace& other);
  bool operator<(const PolicyNamespace& other) const;
  bool operator==(const PolicyNamespace& other) const;
  bool operator!=(const PolicyNamespace& other) const;

  PolicyDomain domain;
  std::string component_id;
};

// The PolicyService merges policies from all available sources, taking into
// account their priorities. Policy clients can retrieve policy for their domain
// and register for notifications on policy updates.
//
// The PolicyService is available from BrowserProcess as a global singleton.
// There is also a PolicyService for browser-wide policies available from
// BrowserProcess as a global singleton.
class PolicyService {
 public:
  class Observer {
   public:
    // Invoked whenever policies for the given |ns| namespace are modified.
    // This is only invoked for changes that happen after AddObserver is called.
    // |previous| contains the values of the policies before the update,
    // and |current| contains the current values.
    virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                                 const PolicyMap& previous,
                                 const PolicyMap& current) = 0;

    // Invoked at most once for each |domain|, when the PolicyService becomes
    // ready. If IsInitializationComplete() is false, then this will be invoked
    // once all the policy providers have finished loading their policies for
    // |domain|.
    virtual void OnPolicyServiceInitialized(PolicyDomain domain) {}

   protected:
    virtual ~Observer() {}
  };

  virtual ~PolicyService() {}

  // Observes changes to all components of the given |domain|.
  virtual void AddObserver(PolicyDomain domain, Observer* observer) = 0;

  virtual void RemoveObserver(PolicyDomain domain, Observer* observer) = 0;

  // Registers the current components of the given policy |domain|, signaling
  // that there is interest in receiving policy for them.
  // |component_ids| is the current set of components for that |domain|, and
  // replaces any previously registered set.
  // The policy providers will try to load policy for these components, and may
  // flush cached data for components that aren't registered anymore.
  virtual void RegisterPolicyDomain(
      PolicyDomain domain,
      const std::set<std::string>& component_ids) = 0;

  virtual const PolicyMap& GetPolicies(const PolicyNamespace& ns) const = 0;

  // The PolicyService loads policy from several sources, and some require
  // asynchronous loads. IsInitializationComplete() returns true once all
  // sources have loaded their policies for the given |domain|.
  // It is safe to read policy from the PolicyService even if
  // IsInitializationComplete() is false; there will be an OnPolicyUpdated()
  // notification once new policies become available.
  //
  // OnPolicyServiceInitialized() is called when IsInitializationComplete()
  // becomes true, which happens at most once for each domain.
  // If IsInitializationComplete() is already true for |domain| when an Observer
  // is registered, then that Observer will not receive an
  // OnPolicyServiceInitialized() notification.
  virtual bool IsInitializationComplete(PolicyDomain domain) const = 0;

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
                        const PolicyNamespace& ns);

  virtual ~PolicyChangeRegistrar();

  // Will invoke |callback| whenever |policy_name| changes its value, as long
  // as this registrar exists.
  // Only one callback can be registed per policy name; a second call with the
  // same |policy_name| will overwrite the previous callback.
  void Observe(const std::string& policy_name, const UpdateCallback& callback);

  // Implementation of PolicyService::Observer:
  virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;

 private:
  typedef std::map<std::string, UpdateCallback> CallbackMap;

  PolicyService* policy_service_;
  PolicyNamespace ns_;
  CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyChangeRegistrar);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
