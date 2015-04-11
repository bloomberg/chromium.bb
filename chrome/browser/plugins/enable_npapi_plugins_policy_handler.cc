// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/enable_npapi_plugins_policy_handler.h"

#include "base/logging.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

EnableNpapiPluginsPolicyHandler::EnableNpapiPluginsPolicyHandler() {
}

EnableNpapiPluginsPolicyHandler::~EnableNpapiPluginsPolicyHandler() {
}

void EnableNpapiPluginsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const std::string plugin_policies[] = {key::kEnabledPlugins,
                                         key::kPluginsAllowedForUrls,
                                         key::kPluginsBlockedForUrls,
                                         key::kDisabledPluginsExceptions,
                                         key::kDisabledPlugins};

  for (auto policy : plugin_policies) {
    if (policies.GetValue(policy)) {
      prefs->SetBoolean(prefs::kEnableNpapi, true);
      break;
    }
  }
}

bool EnableNpapiPluginsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* prefs) {
  return true;
}

}  // namespace policy
