// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/policy/policy_map.h"

namespace policy {

// Policies are namespaced by a (PolicyDomain, ID) pair. The meaning of the ID
// string depends on the domain; for example, if the PolicyDomain is
// "extensions" then the ID identifies the extension that the policies control.
// Currently CHROME is the only domain available, and its ID is always the empty
// string.
enum PolicyDomain {
  POLICY_DOMAIN_CHROME,
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
    // is called.
    virtual void OnPolicyUpdated(PolicyDomain domain,
                                 const std::string& component_id) = 0;

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
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_H_
