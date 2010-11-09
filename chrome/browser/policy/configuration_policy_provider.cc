// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/common/notification_service.h"

namespace policy {

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const PolicyDefinitionList* policy_list)
    : policy_definition_list_(policy_list) {
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {}

void ConfigurationPolicyProvider::NotifyStoreOfPolicyChange() {
  NotificationService::current()->Notify(
      NotificationType::POLICY_CHANGED,
      Source<ConfigurationPolicyProvider>(this),
      NotificationService::NoDetails());
}

void ConfigurationPolicyProvider::DecodePolicyValueTree(
    DictionaryValue* policies,
    ConfigurationPolicyStoreInterface* store) {
  const PolicyDefinitionList* policy_list(policy_definition_list());
  for (const PolicyDefinitionList::Entry* i = policy_list->begin;
       i != policy_list->end; ++i) {
    Value* value;
    if (policies->Get(i->name, &value) && value->IsType(i->value_type))
      store->Apply(i->policy_type, value->DeepCopy());
  }

  // TODO(mnissler): Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}

}  // namespace policy
