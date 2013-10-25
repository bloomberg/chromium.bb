// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_URL_BLACKLIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_URL_BLACKLIST_POLICY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/policy/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Handles URLBlacklist policies.
class URLBlacklistPolicyHandler : public ConfigurationPolicyHandler {
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

#endif  // CHROME_BROWSER_POLICY_URL_BLACKLIST_POLICY_HANDLER_H_
