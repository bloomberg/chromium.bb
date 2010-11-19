// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/common/pref_store.h"

class Profile;

namespace policy {

// An implementation of the |PrefStore| that holds a Dictionary
// created through applied policy.
class ConfigurationPolicyPrefStore : public PrefStore,
                                     public ConfigurationPolicyStoreInterface {
 public:
  typedef std::set<const char*> ProxyPreferenceSet;

  // The ConfigurationPolicyPrefStore does not take ownership of the
  // passed-in |provider|.
  explicit ConfigurationPolicyPrefStore(ConfigurationPolicyProvider* provider);
  virtual ~ConfigurationPolicyPrefStore() {}

  // PrefStore methods:
  virtual PrefReadError ReadPrefs();
  virtual DictionaryValue* prefs() const { return prefs_.get(); }

  // ConfigurationPolicyStore methods:
  virtual void Apply(ConfigurationPolicyType setting, Value* value);

  // Creates a ConfigurationPolicyPrefStore that reads managed platform policy.
  static ConfigurationPolicyPrefStore* CreateManagedPlatformPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that supplies policy from
  // the device management server.
  static ConfigurationPolicyPrefStore* CreateDeviceManagementPolicyPrefStore(
      Profile* profile);

  // Creates a ConfigurationPolicyPrefStore that reads recommended policy.
  static ConfigurationPolicyPrefStore* CreateRecommendedPolicyPrefStore();

  // Returns the default policy definition list for Chrome.
  static const ConfigurationPolicyProvider::PolicyDefinitionList*
      GetChromePolicyDefinitionList();

  // Returns the set of preference paths that can be affected by a proxy
  // policy.
  static void GetProxyPreferenceSet(ProxyPreferenceSet* proxy_pref_set);

 private:
  // Policies that map to a single preference are handled
  // by an automated converter. Each one of these policies
  // has an entry in |simple_policy_map_| with the following type.
  struct PolicyToPreferenceMapEntry {
    Value::ValueType value_type;
    ConfigurationPolicyType policy_type;
    const char* preference_path;  // A DictionaryValue path, not a file path.
  };

  static const PolicyToPreferenceMapEntry kSimplePolicyMap[];
  static const PolicyToPreferenceMapEntry kProxyPolicyMap[];
  static const PolicyToPreferenceMapEntry kDefaultSearchPolicyMap[];
  static const ConfigurationPolicyProvider::PolicyDefinitionList
      kPolicyDefinitionList;

  ConfigurationPolicyProvider* provider_;
  scoped_ptr<DictionaryValue> prefs_;

  // Set to false until the first proxy-relevant policy is applied. At that
  // time, default values are provided for all proxy-relevant prefs
  // to override any values set from stores with a lower priority.
  bool lower_priority_proxy_settings_overridden_;

  // The following are used to track what proxy-relevant policy has been applied
  // accross calls to Apply to provide a warning if a policy specifies a
  // contradictory proxy configuration. |proxy_disabled_| is set to true if and
  // only if the kPolicyNoProxyServer has been applied,
  // |proxy_configuration_specified_| is set to true if and only if any other
  // proxy policy other than kPolicyNoProxyServer has been applied.
  bool proxy_disabled_;
  bool proxy_configuration_specified_;

  // Set to true if a the proxy mode policy has been set to force Chrome
  // to use the system proxy.
  bool use_system_proxy_;

  // Returns the map entry that corresponds to |policy| in the map.
  const PolicyToPreferenceMapEntry* FindPolicyInMap(
      ConfigurationPolicyType policy,
      const PolicyToPreferenceMapEntry* map,
      int size) const;

  // Remove the preferences found in the map from |prefs_|.  Returns true if
  // any such preferences were found and removed.
  bool RemovePreferencesOfMap(const PolicyToPreferenceMapEntry* map,
                              int table_size);

  bool ApplyPolicyFromMap(ConfigurationPolicyType policy,
                          Value* value,
                          const PolicyToPreferenceMapEntry* map,
                          int size);

  // Processes proxy-specific policies. Returns true if the specified policy
  // is a proxy-related policy. ApplyProxyPolicy assumes the ownership
  // of |value| in the case that the policy is proxy-specific.
  bool ApplyProxyPolicy(ConfigurationPolicyType policy, Value* value);

  // Handles sync-related policies. Returns true if the policy was handled.
  // Assumes ownership of |value| in that case.
  bool ApplySyncPolicy(ConfigurationPolicyType policy, Value* value);

  // Handles policies that affect AutoFill. Returns true if the policy was
  // handled and assumes ownership of |value| in that case.
  bool ApplyAutoFillPolicy(ConfigurationPolicyType policy, Value* value);

  // Make sure that the |path| if present in |prefs_|.  If not, set it to
  // a blank string.
  void EnsureStringPrefExists(const std::string& path);

  // If the required entries for default search are specified and valid,
  // finalizes the policy-specified configuration by initializing the
  // unspecified map entries.  Otherwise wipes all default search related
  // map entries from |prefs_|.
  void FinalizeDefaultSearchPolicySettings();

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
