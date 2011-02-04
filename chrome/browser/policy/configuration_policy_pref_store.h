// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/common/pref_store.h"

class Profile;

namespace policy {

class ConfigurationPolicyPrefKeeper;

// An implementation of PrefStore that bridges policy settings as read from a
// ConfigurationPolicyProvider to preferences.
class ConfigurationPolicyPrefStore
    : public PrefStore,
      public ConfigurationPolicyProvider::Observer {
 public:
  // The ConfigurationPolicyPrefStore does not take ownership of the
  // passed-in |provider|.
  explicit ConfigurationPolicyPrefStore(ConfigurationPolicyProvider* provider);
  virtual ~ConfigurationPolicyPrefStore();

  // PrefStore methods:
  virtual void AddObserver(PrefStore::Observer* observer);
  virtual void RemoveObserver(PrefStore::Observer* observer);
  virtual bool IsInitializationComplete() const;
  virtual ReadResult GetValue(const std::string& key, Value** result) const;

  // ConfigurationPolicyProvider::Observer methods:
  virtual void OnUpdatePolicy();
  virtual void OnProviderGoingAway();

  // Creates a ConfigurationPolicyPrefStore that reads managed platform policy.
  static ConfigurationPolicyPrefStore* CreateManagedPlatformPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads managed cloud policy.
  static ConfigurationPolicyPrefStore* CreateManagedCloudPolicyPrefStore(
      Profile* profile);

  // Creates a ConfigurationPolicyPrefStore that reads recommended platform
  // policy.
  static ConfigurationPolicyPrefStore*
      CreateRecommendedPlatformPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads recommended cloud policy.
  static ConfigurationPolicyPrefStore* CreateRecommendedCloudPolicyPrefStore(
      Profile* profile);

  // Returns the default policy definition list for Chrome.
  static const ConfigurationPolicyProvider::PolicyDefinitionList*
      GetChromePolicyDefinitionList();

 private:
  // Refreshes policy information, rereading policy from the provider and
  // sending out change notifications as appropriate.
  void Refresh();

  static const ConfigurationPolicyProvider::PolicyDefinitionList
      kPolicyDefinitionList;

  // The policy provider from which policy settings are read.
  ConfigurationPolicyProvider* provider_;

  // Initialization status as reported by the policy provider the last time we
  // queried it.
  bool initialization_complete_;

  // Current policy preferences.
  scoped_ptr<ConfigurationPolicyPrefKeeper> policy_keeper_;

  ObserverList<PrefStore::Observer, true> observers_;

  ConfigurationPolicyObserverRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
