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

}  // namespace policy
