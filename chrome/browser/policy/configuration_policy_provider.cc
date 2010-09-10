// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/common/policy_constants.h"
#include "chrome/common/notification_service.h"

namespace policy {

// TODO(avi): Generate this mapping from the template metafile
// (chrome/app/policy/policy_templates.json). http://crbug.com/54711

struct InternalPolicyValueMapEntry {
  ConfigurationPolicyStore::PolicyType policy_type;
  Value::ValueType value_type;
  const char* name;
};

const InternalPolicyValueMapEntry kPolicyValueMap[] = {
  { ConfigurationPolicyStore::kPolicyHomePage,
      Value::TYPE_STRING, key::kHomepageLocation },
  { ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
      Value::TYPE_BOOLEAN, key::kHomepageIsNewTabPage },
  { ConfigurationPolicyStore::kPolicyRestoreOnStartup,
      Value::TYPE_INTEGER, key::kRestoreOnStartup },
  { ConfigurationPolicyStore::kPolicyURLsToRestoreOnStartup,
      Value::TYPE_LIST, key::kURLsToRestoreOnStartup },
  { ConfigurationPolicyStore::kPolicyProxyServerMode,
      Value::TYPE_INTEGER, key::kProxyServerMode },
  { ConfigurationPolicyStore::kPolicyProxyServer,
      Value::TYPE_STRING, key::kProxyServer },
  { ConfigurationPolicyStore::kPolicyProxyPacUrl,
      Value::TYPE_STRING, key::kProxyPacUrl },
  { ConfigurationPolicyStore::kPolicyProxyBypassList,
      Value::TYPE_STRING, key::kProxyBypassList },
  { ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
      Value::TYPE_BOOLEAN, key::kAlternateErrorPagesEnabled },
  { ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
      Value::TYPE_BOOLEAN, key::kSearchSuggestEnabled },
  { ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
      Value::TYPE_BOOLEAN, key::kDnsPrefetchingEnabled },
  { ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
      Value::TYPE_BOOLEAN, key::kSafeBrowsingEnabled },
  { ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
      Value::TYPE_BOOLEAN, key::kMetricsReportingEnabled },
  { ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
      Value::TYPE_BOOLEAN, key::kPasswordManagerEnabled },
  { ConfigurationPolicyStore::kPolicyPasswordManagerAllowShowPasswords,
      Value::TYPE_BOOLEAN, key::kPasswordManagerAllowShowPasswords },
  { ConfigurationPolicyStore::kPolicyAutoFillEnabled,
      Value::TYPE_BOOLEAN, key::kAutoFillEnabled },
  { ConfigurationPolicyStore::kPolicyDisabledPlugins,
      Value::TYPE_LIST, key::kDisabledPlugins },
  { ConfigurationPolicyStore::kPolicyApplicationLocale,
      Value::TYPE_STRING, key::kApplicationLocaleValue },
  { ConfigurationPolicyStore::kPolicySyncDisabled,
      Value::TYPE_BOOLEAN, key::kSyncDisabled },
  { ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
      Value::TYPE_LIST, key::kExtensionInstallAllowList },
  { ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
      Value::TYPE_LIST, key::kExtensionInstallDenyList },
  { ConfigurationPolicyStore::kPolicyShowHomeButton,
      Value::TYPE_BOOLEAN, key::kShowHomeButton },
  { ConfigurationPolicyStore::kPolicyPrintingEnabled,
      Value::TYPE_BOOLEAN, key::kPrintingEnabled },
};

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

}  // namespace policy
