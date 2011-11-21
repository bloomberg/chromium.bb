// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/common/pref_store.h"

class PrefValueMap;

namespace policy {

// Constants for the "Proxy Server Mode" defined in the policies.
// Note that these diverge from internal presentation defined in
// ProxyPrefs::ProxyMode for legacy reasons. The following four
// PolicyProxyModeType types were not very precise and had overlapping use
// cases.
enum PolicyProxyModeType {
  // Disable Proxy, connect directly.
  kPolicyNoProxyServerMode = 0,
  // Auto detect proxy or use specific PAC script if given.
  kPolicyAutoDetectProxyServerMode = 1,
  // Use manually configured proxy servers (fixed servers).
  kPolicyManuallyConfiguredProxyServerMode = 2,
  // Use system proxy server.
  kPolicyUseSystemProxyServerMode = 3,

  MODE_COUNT
};

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
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;
  virtual ReadResult GetValue(const std::string& key,
                              const Value** result) const OVERRIDE;

  // ConfigurationPolicyProvider::Observer methods:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;
  virtual void OnProviderGoingAway(
      ConfigurationPolicyProvider* provider) OVERRIDE;

  // Creates a ConfigurationPolicyPrefStore that reads managed platform policy.
  static ConfigurationPolicyPrefStore* CreateManagedPlatformPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads managed cloud policy.
  static ConfigurationPolicyPrefStore* CreateManagedCloudPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads recommended platform
  // policy.
  static ConfigurationPolicyPrefStore*
      CreateRecommendedPlatformPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads recommended cloud policy.
  static ConfigurationPolicyPrefStore* CreateRecommendedCloudPolicyPrefStore();

  // Returns true if the given policy is a proxy policy.
  static bool IsProxyPolicy(ConfigurationPolicyType policy);

 private:
  // Refreshes policy information, rereading policy from the provider and
  // sending out change notifications as appropriate.
  void Refresh();

  // Returns a new PrefValueMap containing the preference values that correspond
  // to the policies currently provided by |provider_|.
  PrefValueMap* CreatePreferencesFromPolicies();

  // The policy provider from which policy settings are read.
  ConfigurationPolicyProvider* provider_;

  // Initialization status as reported by the policy provider the last time we
  // queried it.
  bool initialization_complete_;

  // Current policy preferences.
  scoped_ptr<PrefValueMap> prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  ConfigurationPolicyObserverRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
