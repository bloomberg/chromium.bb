// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider.h"

#include "base/values.h"
#include "chrome/common/notification_service.h"

namespace policy {

ConfigurationPolicyProvider::ConfigurationPolicyProvider(
    const StaticPolicyValueMap& policy_map) {
  for (size_t i = 0; i < policy_map.entry_count; ++i) {
    PolicyValueMapEntry entry = {
      policy_map.entries[i].policy_type,
      policy_map.entries[i].value_type,
      std::string(policy_map.entries[i].name)
    };
    policy_value_map_.push_back(entry);
  }
}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {}

void ConfigurationPolicyProvider::NotifyStoreOfPolicyChange() {
  NotificationService::current()->Notify(
      NotificationType::POLICY_CHANGED,
      Source<ConfigurationPolicyProvider>(this),
      NotificationService::NoDetails());
}

}  // namespace policy
