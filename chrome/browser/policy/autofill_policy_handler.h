// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_AUTOFILL_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_AUTOFILL_POLICY_HANDLER_H_

#include "chrome/browser/policy/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// ConfigurationPolicyHandler for the AutofillEnabled policy.
class AutofillPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  AutofillPolicyHandler();
  virtual ~AutofillPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_AUTOFILL_POLICY_HANDLER_H_
