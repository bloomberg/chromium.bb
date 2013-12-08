// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_ANDROID_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_ANDROID_H_

#include <string>

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace base {
class Value;
}

namespace policy {

// Handles the ManagedBookmarks policy.
class ManagedBookmarksPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  static const char kName[];
  static const char kUrl[];

  ManagedBookmarksPolicyHandler();
  virtual ~ManagedBookmarksPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_ANDROID_H_
