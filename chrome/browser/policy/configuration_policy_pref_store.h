// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_store.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"

class PrefValueMap;

namespace policy {

// An implementation of PrefStore that bridges policy settings as read from the
// PolicyService to preferences. Converts POLICY_DOMAIN_CHROME policies a given
// PolicyLevel to their corresponding preferences.
class ConfigurationPolicyPrefStore
    : public PrefStore,
      public PolicyService::Observer {
 public:
  // Does not take ownership of |service|. Only policies of the given |level|
  // will be mapped.
  ConfigurationPolicyPrefStore(PolicyService* service, PolicyLevel level);

  // PrefStore methods:
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual size_t NumberOfObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual bool GetValue(const std::string& key,
                        const Value** result) const OVERRIDE;

  // PolicyService::Observer methods:
  virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;
  virtual void OnPolicyServiceInitialized(PolicyDomain domain) OVERRIDE;

  // Creates a ConfigurationPolicyPrefStore that only provides policies that
  // have POLICY_LEVEL_MANDATORY level.
  static ConfigurationPolicyPrefStore* CreateMandatoryPolicyPrefStore(
      PolicyService* policy_service);

  // Creates a ConfigurationPolicyPrefStore that only provides policies that
  // have POLICY_LEVEL_RECOMMENDED level.
  static ConfigurationPolicyPrefStore* CreateRecommendedPolicyPrefStore(
      PolicyService* policy_service);

 private:
  virtual ~ConfigurationPolicyPrefStore();

  // Refreshes policy information, rereading policy from the policy service and
  // sending out change notifications as appropriate.
  void Refresh();

  // Returns a new PrefValueMap containing the preference values that correspond
  // to the policies currently provided by the policy service.
  PrefValueMap* CreatePreferencesFromPolicies();

  // The PolicyService from which policy settings are read.
  PolicyService* policy_service_;

  // The policy level that this PrefStore uses.
  PolicyLevel level_;

  // Current policy preferences.
  scoped_ptr<PrefValueMap> prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
