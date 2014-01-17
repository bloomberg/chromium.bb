// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_POLICY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace policy {

// Handles URLBlacklist policies.
class POLICY_EXPORT URLBlacklistPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  URLBlacklistPolicyHandler();
  virtual ~URLBlacklistPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLBlacklistPolicyHandler);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_POLICY_HANDLER_H_
