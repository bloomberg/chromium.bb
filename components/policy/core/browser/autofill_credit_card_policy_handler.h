// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace policy {

// ConfigurationPolicyHandler for the AutofillCreditCardEnabled policy.
class POLICY_EXPORT AutofillCreditCardPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  AutofillCreditCardPolicyHandler();
  ~AutofillCreditCardPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardPolicyHandler);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_
