// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/autofill_credit_card_policy_handler.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test cases for the Autofill credit card policy setting.
class AutofillCreditCardPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillCreditCardPolicyHandlerTest, Default) {
  PolicyMap policy;
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, NULL));
}

TEST_F(AutofillCreditCardPolicyHandlerTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kAutofillCreditCardEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>(true), nullptr);
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill for credit cards should not set the prefs.
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, NULL));
}

TEST_F(AutofillCreditCardPolicyHandlerTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kAutofillCreditCardEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>(false), nullptr);
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill should switch the prefs to managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_credt_card_enabled = true;
  bool result = value->GetAsBoolean(&autofill_credt_card_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_credt_card_enabled);
}

}  // namespace policy
