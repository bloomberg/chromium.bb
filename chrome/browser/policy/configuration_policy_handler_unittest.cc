// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/common/pref_names.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(ExtensionListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  ExtensionListPolicyHandler handler(key::kExtensionInstallBlacklist,
                                     prefs::kExtensionInstallDenyList,
                                     true);

  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("abcdefghijklmnopabcdefghijklmnop"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("*"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("invalid"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallBlacklist).empty());
}

TEST(ExtensionListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionListPolicyHandler handler(key::kExtensionInstallBlacklist,
                                     prefs::kExtensionInstallDenyList,
                                     false);

  list.Append(Value::CreateStringValue("abcdefghijklmnopabcdefghijklmnop"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kExtensionInstallDenyList, &value));
  EXPECT_TRUE(base::Value::Equals(&list, value));
}

TEST(ExtensionURLPatternListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  ExtensionURLPatternListPolicyHandler handler(
      key::kExtensionInstallSources,
      prefs::kExtensionAllowedInstallSites);

  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("http://*.google.com/*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("<all_urls>"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("invalid"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallSources).empty());

  // URLPattern syntax has a different way to express 'all urls'. Though '*'
  // would be compatible today, it would be brittle, so we disallow.
  list.Append(Value::CreateStringValue("*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallSources).empty());
}

TEST(ExtensionURLPatternListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionURLPatternListPolicyHandler handler(
      key::kExtensionInstallSources,
      prefs::kExtensionAllowedInstallSites);

  list.Append(Value::CreateStringValue("https://corp.monkey.net/*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy());
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kExtensionAllowedInstallSites, &value));
  EXPECT_TRUE(base::Value::Equals(&list, value));
}

}  // namespace policy
