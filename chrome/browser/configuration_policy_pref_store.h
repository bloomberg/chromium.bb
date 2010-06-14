// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_
#define CHROME_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/configuration_policy_provider.h"
#include "chrome/browser/configuration_policy_store.h"
#include "chrome/common/pref_store.h"

// An implementation of the |PrefStore| that holds a Dictionary
// created through applied policy.
class ConfigurationPolicyPrefStore : public PrefStore,
                                     public ConfigurationPolicyStore {
 public:
  // The ConfigurationPolicyPrefStore takes the ownership of the passed
  // |provider|.
  explicit ConfigurationPolicyPrefStore(ConfigurationPolicyProvider* provider);
  virtual ~ConfigurationPolicyPrefStore() { }

  // PrefStore methods:
  virtual PrefReadError ReadPrefs();
  virtual DictionaryValue* prefs() { return prefs_.get(); }

 private:
  // For unit tests.
  friend class ConfigurationPolicyPrefStoreTest;

  // Policies that map to a single preference are handled
  // by an automated converter. Each one of these policies
  // has an entry in |simple_policy_map_| with the following type.
  struct PolicyToPreferenceMapEntry {
    Value::ValueType value_type;
    PolicyType policy_type;
    const wchar_t* preference_path;  // A DictionaryValue path, not a file path.
  };

  static const PolicyToPreferenceMapEntry simple_policy_map_[];

  scoped_ptr<ConfigurationPolicyProvider> provider_;
  scoped_ptr<DictionaryValue> prefs_;

  // ConfigurationPolicyStore methods:
  virtual void Apply(PolicyType setting, Value* value);

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStore);
};

#endif  // CHROME_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_

