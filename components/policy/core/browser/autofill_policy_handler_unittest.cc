// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/browser/autofill_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test cases for the Autofill policy setting.
class AutofillPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillPolicyHandlerTest, Default) {
  PolicyMap policy;
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(prefs.GetValue(autofill::prefs::kAutofillEnabled, NULL));
}

TEST_F(AutofillPolicyHandlerTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             new base::FundamentalValue(true),
             NULL);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill should not set the pref.
  EXPECT_FALSE(prefs.GetValue(autofill::prefs::kAutofillEnabled, NULL));
}

TEST_F(AutofillPolicyHandlerTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             new base::FundamentalValue(false),
             NULL);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill should switch the pref to managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(prefs.GetValue(autofill::prefs::kAutofillEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_enabled = true;
  bool result = value->GetAsBoolean(&autofill_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_enabled);
}

}  // namespace policy
