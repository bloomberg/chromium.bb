// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider.h"

#include "base/values.h"

namespace {

// TODO(avi): Use this mapping to auto-generate MCX manifests and Windows
// ADM/ADMX files. http://crbug.com/45334

struct InternalPolicyValueMapEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const char* name;
};

const InternalPolicyValueMapEntry kPolicyValueMap[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
    Value::TYPE_STRING, "Homepage" },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
    Value::TYPE_BOOLEAN, "HomepageIsNewTabPage" },
  { ConfigurationPolicyStore::kPolicyCookiesMode,
    Value::TYPE_INTEGER, "CookiesMode" }
};

}  // namespace

/* static */
const ConfigurationPolicyProvider::PolicyValueMap*
    ConfigurationPolicyProvider::PolicyValueMapping() {
  static PolicyValueMap* mapping;
  if (!mapping) {
    mapping = new PolicyValueMap();
    for (size_t i = 0; i < arraysize(kPolicyValueMap); ++i) {
      const InternalPolicyValueMapEntry& internal_entry = kPolicyValueMap[i];
      PolicyValueMapEntry entry;
      entry.policy_type = internal_entry.policy_type;
      entry.value_type = internal_entry.value_type;
      entry.name = std::string(internal_entry.name);
      mapping->push_back(entry);
    }
  }
  return mapping;
}
