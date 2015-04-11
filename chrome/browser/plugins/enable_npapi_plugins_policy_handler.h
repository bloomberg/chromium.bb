// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_ENABLE_NPAPI_PLUGINS_POLICY_HANDLER_H_
#define CHROME_BROWSER_PLUGINS_ENABLE_NPAPI_PLUGINS_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// ConfigurationPolicyHandler for the plugin policies that re-enable NPAPI.
class EnableNpapiPluginsPolicyHandler : public ConfigurationPolicyHandler {
 public:
  EnableNpapiPluginsPolicyHandler();

  ~EnableNpapiPluginsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* prefs) override;

  DISALLOW_COPY_AND_ASSIGN(EnableNpapiPluginsPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_PLUGINS_ENABLE_NPAPI_PLUGINS_POLICY_HANDLER_H_
