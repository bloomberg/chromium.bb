// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_pref_store.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/configuration_policy_provider.h"
#include "chrome/common/pref_names.h"

const ConfigurationPolicyPrefStore::PolicyToPreferenceMapEntry
ConfigurationPolicyPrefStore::simple_policy_map_[] = {
  { Value::TYPE_STRING, kPolicyHomePage,  prefs::kHomePage },
  { Value::TYPE_BOOLEAN, kPolicyHomepageIsNewTabPage,
      prefs::kHomePageIsNewTabPage },
  { Value::TYPE_INTEGER, kPolicyCookiesMode, prefs::kCookieBehavior }
};

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    ConfigurationPolicyProvider* provider)
    : provider_(provider),
      prefs_(new DictionaryValue()) {
}

PrefStore::PrefReadError ConfigurationPolicyPrefStore::ReadPrefs() {
  return provider_->Provide(this) ? PrefStore::PREF_READ_ERROR_NONE :
      PrefStore::PREF_READ_ERROR_OTHER;
}

void ConfigurationPolicyPrefStore::Apply(PolicyType policy, Value* value) {
  const PolicyToPreferenceMapEntry* end =
      simple_policy_map_ + arraysize(simple_policy_map_);
  for (const PolicyToPreferenceMapEntry* current = simple_policy_map_;
       current != end; ++current) {
    if (current->policy_type == policy) {
      DCHECK(current->value_type == value->GetType());
      prefs_->Set(current->preference_path, value);
      return;
    }
  }

  // Other policy implementations go here.
  NOTIMPLEMENTED();
}

