// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/browser/configuration_policy_store.h"

namespace {

struct PolicyValueTreeMappingEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const wchar_t* const name;
};

const PolicyValueTreeMappingEntry kPolicyValueTreeMapping[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
    Value::TYPE_STRING, L"homepage" },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
    Value::TYPE_BOOLEAN, L"homepage_is_newtabpage" },
  { ConfigurationPolicyStore::kPolicyCookiesMode,
    Value::TYPE_INTEGER, L"cookies_enabled" }
};

}

void DecodePolicyValueTree(DictionaryValue* policies,
                           ConfigurationPolicyStore* store) {
  for (size_t i = 0; i < arraysize(kPolicyValueTreeMapping); ++i) {
    const PolicyValueTreeMappingEntry& entry(kPolicyValueTreeMapping[i]);
    Value* value;
    if (policies->Get(entry.name, &value) && value->IsType(entry.value_type))
      store->Apply(entry.policy_type, value->DeepCopy());
  }

  // TODO (mnissler) Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}
