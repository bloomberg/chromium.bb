// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

class JavascriptPolicyHandlerTest : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
          new JavascriptPolicyHandler));
  }
};

TEST_F(JavascriptPolicyHandlerTest, JavascriptEnabled) {
  // This is a boolean policy, but affects an integer preference.
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(true),
             NULL);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  policy.Set(key::kJavascriptEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(false),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
}

TEST_F(JavascriptPolicyHandlerTest, JavascriptEnabledOverridden) {
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(false),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
  // DefaultJavaScriptSetting overrides JavascriptEnabled.
  policy.Set(key::kDefaultJavaScriptSetting,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(CONTENT_SETTING_ALLOW),
             NULL);
  UpdateProviderPolicy(policy);
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_ALLOW).Equals(value));
}

}  // namespace policy
