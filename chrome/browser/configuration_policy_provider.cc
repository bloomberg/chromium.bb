// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider.h"

#include "base/values.h"

namespace {

// TODO(avi): Use this mapping to auto-generate MCX manifests and Windows
// ADM/ADMX files. http://crbug.com/49316

struct InternalPolicyValueMapEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const char* name;
};

const InternalPolicyValueMapEntry kPolicyValueMap[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
      Value::TYPE_STRING, "HomepageLocation" },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
      Value::TYPE_BOOLEAN, "HomepageIsNewTabPage" },
  { ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::TYPE_INTEGER, "ProxyServerMode" },
  { ConfigurationPolicyStore::kPolicyProxyServer,
      Value::TYPE_STRING, "ProxyServer" },
  { ConfigurationPolicyStore::kPolicyProxyPacUrl,
      Value::TYPE_STRING, "ProxyPacUrl" },
  { ConfigurationPolicyStore::kPolicyProxyBypassList,
       Value::TYPE_STRING, "ProxyBypassList" },
  { ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
      Value::TYPE_BOOLEAN, "AlternateErrorPagesEnabled" },
  { ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
      Value::TYPE_BOOLEAN, "SearchSuggestEnabled" },
  { ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
      Value::TYPE_BOOLEAN, "DnsPrefetchingEnabled" },
  { ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
      Value::TYPE_BOOLEAN, "SafeBrowsingEnabled" },
  { ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
      Value::TYPE_BOOLEAN, "MetricsReportingEnabled" },
  { ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
      Value::TYPE_BOOLEAN, "PasswordManagerEnabled" },
  { ConfigurationPolicyStore::kPolicyDisabledPlugins,
      Value::TYPE_STRING, "DisabledPluginsList" },
  { ConfigurationPolicyStore::kPolicyApplicationLocale,
      Value::TYPE_STRING, "ApplicationLocaleValue" },
  { ConfigurationPolicyStore::kPolicySyncDisabled,
      Value::TYPE_BOOLEAN, "SyncDisabled" },
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
