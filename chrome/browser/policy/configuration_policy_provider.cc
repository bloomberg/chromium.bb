// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/common/policy_constants.h"
#include "chrome/common/notification_service.h"

namespace {

// TODO(avi): Generate this mapping from the template metafile
// (chrome/app/policy/policy_templates.json). http://crbug.com/54711

struct InternalPolicyValueMapEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const char* name;
};

const InternalPolicyValueMapEntry kPolicyValueMap[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
      Value::TYPE_STRING, policy::key::kHomepageLocation },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
      Value::TYPE_BOOLEAN, policy::key::kHomepageIsNewTabPage },
  { ConfigurationPolicyStore::kPolicyRestoreOnStartup,
      Value::TYPE_INTEGER, policy::key::kRestoreOnStartup },
  { ConfigurationPolicyStore::kPolicyURLsToRestoreOnStartup,
      Value::TYPE_LIST, policy::key::kURLsToRestoreOnStartup },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderName },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderKeyword },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderSearchURL },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderSuggestURL },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderIconURL },
  { ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
      Value::TYPE_STRING, policy::key::kDefaultSearchProviderEncodings },
  { ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::TYPE_INTEGER, policy::key::kProxyServerMode },
  { ConfigurationPolicyStore::kPolicyProxyServer,
      Value::TYPE_STRING, policy::key::kProxyServer },
  { ConfigurationPolicyStore::kPolicyProxyPacUrl,
      Value::TYPE_STRING, policy::key::kProxyPacUrl },
  { ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::TYPE_STRING, policy::key::kProxyBypassList },
  { ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
      Value::TYPE_BOOLEAN, policy::key::kAlternateErrorPagesEnabled },
  { ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
      Value::TYPE_BOOLEAN, policy::key::kSearchSuggestEnabled },
  { ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kDnsPrefetchingEnabled },
  { ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kSafeBrowsingEnabled },
  { ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kMetricsReportingEnabled },
  { ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
      Value::TYPE_BOOLEAN, policy::key::kPasswordManagerEnabled },
  { ConfigurationPolicyStore::kPolicyPasswordManagerAllowShowPasswords,
      Value::TYPE_BOOLEAN, policy::key::kPasswordManagerAllowShowPasswords },
  { ConfigurationPolicyStore::kPolicyAutoFillEnabled,
      Value::TYPE_BOOLEAN, policy::key::kAutoFillEnabled },
  { ConfigurationPolicyStore::kPolicyDisabledPlugins,
      Value::TYPE_LIST, policy::key::kDisabledPlugins },
  { ConfigurationPolicyStore::kPolicyApplicationLocale,
      Value::TYPE_STRING, policy::key::kApplicationLocaleValue },
  { ConfigurationPolicyStore::kPolicySyncDisabled,
      Value::TYPE_BOOLEAN, policy::key::kSyncDisabled },
  { ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
      Value::TYPE_LIST, policy::key::kExtensionInstallAllowList },
  { ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
      Value::TYPE_LIST, policy::key::kExtensionInstallDenyList },
  { ConfigurationPolicyStore::kPolicyShowHomeButton,
      Value::TYPE_BOOLEAN, policy::key::kShowHomeButton },
  { ConfigurationPolicyStore::kPolicyPrintingEnabled,
      Value::TYPE_BOOLEAN, policy::key::kPrintingEnabled },
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

void ConfigurationPolicyProvider::NotifyStoreOfPolicyChange() {
  NotificationService::current()->Notify(
      NotificationType::POLICY_CHANGED,
      Source<ConfigurationPolicyProvider>(this),
      NotificationService::NoDetails());
}
