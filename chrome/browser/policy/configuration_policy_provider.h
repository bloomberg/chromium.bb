// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_service.h"

namespace policy {

// A mostly-abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class ConfigurationPolicyProvider {
 public:
  class Observer {
   public:
    virtual ~Observer();
    virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) = 0;
  };

  ConfigurationPolicyProvider();

  // Policy providers can be deleted quite late during shutdown of the browser,
  // and it's not guaranteed that the message loops will still be running when
  // this is invoked. Override Shutdown() instead for cleanup code that needs
  // to post to the FILE thread, for example.
  virtual ~ConfigurationPolicyProvider();

  // Invoked as soon as the main message loops are spinning. Policy providers
  // are created early during startup to provide the initial policies; the
  // Init() call allows them to perform initialization tasks that require
  // running message loops.
  virtual void Init();

  // Must be invoked before deleting the provider. Implementations can override
  // this method to do appropriate cleanup while threads are still running, and
  // must also invoke ConfigurationPolicyProvider::Shutdown().
  // The provider should keep providing the current policies after Shutdown()
  // is invoked, it only has to stop updating.
  virtual void Shutdown();

  // Returns the current PolicyBundle.
  const PolicyBundle& policies() const { return policy_bundle_; }

  // Check whether this provider has completed initialization for the given
  // policy |domain|. This is used to detect whether initialization is done in
  // case implementations need to do asynchronous operations for initialization.
  virtual bool IsInitializationComplete(PolicyDomain domain) const;

  // Asks the provider to refresh its policies. All the updates caused by this
  // call will be visible on the next call of OnUpdatePolicy on the observers,
  // which are guaranteed to happen even if the refresh fails.
  // It is possible that Shutdown() is called first though, and
  // OnUpdatePolicy won't be called if that happens.
  virtual void RefreshPolicies() = 0;

  // Observers must detach themselves before the provider is deleted.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Notifies the provider that there is interest in loading policy for the
  // listed components of the given |domain|. The list is complete; all the
  // components that matter for the domain are included, and components not
  // included can be discarded. The provider can ignore this information or use
  // it to selectively load the corresponding policy from its sources.
  virtual void RegisterPolicyDomain(
      PolicyDomain domain,
      const std::set<std::string>& component_ids);

 protected:
  // Subclasses must invoke this to update the policies currently served by
  // this provider. UpdatePolicy() takes ownership of |policies|.
  // The observers are notified after the policies are updated.
  void UpdatePolicy(scoped_ptr<PolicyBundle> bundle);

 private:
  // The policies currently configured at this provider.
  PolicyBundle policy_bundle_;

  // Whether Shutdown() has been invoked.
  bool did_shutdown_;

  ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
