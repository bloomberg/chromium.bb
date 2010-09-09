// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_store.h"
#include "chrome/common/pref_store.h"

class CommandLine;

// An implementation of the |PrefStore| that holds a Dictionary
// created through applied policy.
class ConfigurationPolicyPrefStore : public PrefStore,
                                     public ConfigurationPolicyStore {
 public:
  // The ConfigurationPolicyPrefStore does not take ownership of the
  // passed-in |provider|.
  ConfigurationPolicyPrefStore(const CommandLine* command_line,
                               ConfigurationPolicyProvider* provider);
  virtual ~ConfigurationPolicyPrefStore() { }

  // PrefStore methods:
  virtual PrefReadError ReadPrefs();
  virtual DictionaryValue* prefs() { return prefs_.get(); }

  // Creates a ConfigurationPolicyPrefStore that reads managed policy.
  static ConfigurationPolicyPrefStore* CreateManagedPolicyPrefStore();

  // Creates a ConfigurationPolicyPrefStore that reads recommended policy.
  static ConfigurationPolicyPrefStore* CreateRecommendedPolicyPrefStore();

 private:
  // For unit tests.
  friend class ConfigurationPolicyPrefStoreTest;

  // Policies that map to a single preference are handled
  // by an automated converter. Each one of these policies
  // has an entry in |simple_policy_map_| with the following type.
  struct PolicyToPreferenceMapEntry {
    Value::ValueType value_type;
    PolicyType policy_type;
    const char* preference_path;  // A DictionaryValue path, not a file path.
  };

  static const PolicyToPreferenceMapEntry simple_policy_map_[];
  static const PolicyToPreferenceMapEntry proxy_policy_map_[];
  static const PolicyToPreferenceMapEntry default_search_policy_map_[];

  const CommandLine* command_line_;
  ConfigurationPolicyProvider* provider_;
  scoped_ptr<DictionaryValue> prefs_;

  // Set to false until the first proxy-relevant policy is applied. Allows
  // the Apply method to erase all switch-specified proxy configuration before
  // applying proxy policy configuration;
  bool command_line_proxy_settings_cleared_;

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

  // ConfigurationPolicyStore methods:
  virtual void Apply(PolicyType setting, Value* value);

  // Initializes default preference values from proxy-related command-line
  // switches in |command_line_|.
  void ApplyProxySwitches();

  bool ApplyPolicyFromMap(PolicyType policy, Value* value,
                          const PolicyToPreferenceMapEntry map[], int size);

  // Returns the map entry that corresponds to |policy| in the map.
  const PolicyToPreferenceMapEntry* FindPolicyInMap(PolicyType policy,
      const PolicyToPreferenceMapEntry* map, int size);

  // Remove the preferences found in the map from |prefs_|.  Returns true if
  // any such preferences were found and removed.
  bool RemovePreferencesOfMap(const PolicyToPreferenceMapEntry* map,
                              int table_size);

  // Processes proxy-specific policies. Returns true if the specified policy
  // is a proxy-related policy. ApplyProxyPolicy assumes the ownership
  // of |value| in the case that the policy is proxy-specific.
  bool ApplyProxyPolicy(PolicyType policy, Value* value);

  // Handles sync-related policies. Returns true if the policy was handled.
  // Assumes ownership of |value| in that case.
  bool ApplySyncPolicy(PolicyType policy, Value* value);

  // Handles policies that affect AutoFill. Returns true if the policy was
  // handled and assumes ownership of |value| in that case.
  bool ApplyAutoFillPolicy(PolicyType policy, Value* value);

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

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_H_
