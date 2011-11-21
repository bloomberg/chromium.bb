// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "policy/configuration_policy_type.h"

namespace policy {

struct PolicyDefinitionList;
class PolicyMap;

// A mostly-abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class ConfigurationPolicyProvider {
 public:
  class Observer {
   public:
    virtual ~Observer();
    virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) = 0;
    virtual void OnProviderGoingAway(ConfigurationPolicyProvider* provider);
  };

  explicit ConfigurationPolicyProvider(const PolicyDefinitionList* policy_list);

  virtual ~ConfigurationPolicyProvider();

  // Fills the given |result| with the current policy values. Returns true if
  // the policies were provided. This is used mainly by the
  // ConfigurationPolicyPrefStore, which retrieves policy values from here.
  bool Provide(PolicyMap* result);

  // Check whether this provider has completed initialization. This is used to
  // detect whether initialization is done in case providers implementations
  // need to do asynchronous operations for initialization.
  virtual bool IsInitializationComplete() const;

#if !defined(OFFICIAL_BUILD)
  // Overrides the policy values that are obtained in calls to |Provide|.
  // The default behavior is restored if |policies| is NULL.
  // Takes ownership of |policies|.
  // This is meant for tests only, and is disabled on official builds.
  void OverridePolicies(PolicyMap* policies);
#endif

  // Asks the provider to refresh its policies. All the updates caused by this
  // call will be visible on the next call of OnUpdatePolicy on the observers,
  // which are guaranteed to happen even if the refresh fails.
  // It is possible that OnProviderGoingAway is called first though, and
  // OnUpdatePolicy won't be called if that happens.
  virtual void RefreshPolicies() = 0;

 protected:
  // Sends a policy update notification to observers.
  void NotifyPolicyUpdated();

  // Must be implemented by subclasses to provide their policy values. The
  // values actually provided by |Provide| can be overridden using
  // |OverridePolicies|.
  virtual bool ProvideInternal(PolicyMap* result) = 0;

  const PolicyDefinitionList* policy_definition_list() const {
    return policy_definition_list_;
  }

 private:
  friend class ConfigurationPolicyObserverRegistrar;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Contains the default mapping from policy values to the actual names.
  const PolicyDefinitionList* policy_definition_list_;

  ObserverList<Observer, true> observer_list_;

#if !defined(OFFICIAL_BUILD)
  // Usually NULL, but can be used in tests to override the policies provided.
  scoped_ptr<PolicyMap> override_policies_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

// Manages observers for a ConfigurationPolicyProvider. Is used to register
// observers, and automatically removes them upon destruction.
// Implementation detail: to avoid duplicate bookkeeping of registered
// observers, this registrar class acts as a proxy for notifications (since it
// needs to register itself anyway to get OnProviderGoingAway notifications).
class ConfigurationPolicyObserverRegistrar
    : ConfigurationPolicyProvider::Observer {
 public:
  ConfigurationPolicyObserverRegistrar();
  virtual ~ConfigurationPolicyObserverRegistrar();
  void Init(ConfigurationPolicyProvider* provider,
            ConfigurationPolicyProvider::Observer* observer);

  // ConfigurationPolicyProvider::Observer implementation:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;
  virtual void OnProviderGoingAway(
      ConfigurationPolicyProvider* provider) OVERRIDE;

  ConfigurationPolicyProvider* provider() { return provider_; }

 private:
  ConfigurationPolicyProvider* provider_;
  ConfigurationPolicyProvider::Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyObserverRegistrar);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
