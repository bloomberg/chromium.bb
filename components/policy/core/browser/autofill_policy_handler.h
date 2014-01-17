// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace policy {

// ConfigurationPolicyHandler for the AutofillEnabled policy.
class POLICY_EXPORT AutofillPolicyHandler : public TypeCheckingPolicyHandler {
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

#endif  // COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_POLICY_HANDLER_H_
