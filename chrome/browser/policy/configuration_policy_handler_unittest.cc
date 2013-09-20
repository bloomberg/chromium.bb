// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

StringToIntEnumListPolicyHandler::MappingEntry kTestTypeMap[] = {
  { "one", 1 },
  { "two", 2 },
};

const char kTestPref[] = "unit_test.test_pref";

}  // namespace

TEST(StringToIntEnumListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  StringToIntEnumListPolicyHandler handler(
      key::kExtensionAllowedTypes,
      kTestPref,
      kTestTypeMap,
      kTestTypeMap + arraysize(kTestTypeMap));

  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("one");
  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("invalid");
  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionAllowedTypes).empty());

  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("no list"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionAllowedTypes).empty());
}

TEST(StringToIntEnumListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  base::ListValue expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value;
  StringToIntEnumListPolicyHandler handler(
      key::kExtensionAllowedTypes,
      kTestPref,
      kTestTypeMap,
      kTestTypeMap + arraysize(kTestTypeMap));

  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  list.AppendString("two");
  expected.AppendInteger(2);
  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  list.AppendString("invalid");
  policy_map.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

TEST(IntRangePolicyHandler, CheckPolicySettingsClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(key::kDiskCacheSize,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("invalid"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntRangePolicyHandler, CheckPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(key::kDiskCacheSize, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("invalid"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntRangePolicyHandler, ApplyPolicySettingsClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  scoped_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(5));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(10));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(10));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));
}

TEST(IntRangePolicyHandler, ApplyPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  scoped_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(5));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(10));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));
}

TEST(IntPercentageToDoublePolicyHandler, CheckPolicySettingsClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("invalid"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntPercentageToDoublePolicyHandler, CheckPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      key::kDiskCacheSize, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("invalid"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntPercentageToDoublePolicyHandler, ApplyPolicySettingsClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  scoped_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.05));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.1));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.1));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));
}

TEST(IntPercentageToDoublePolicyHandler, ApplyPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  scoped_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      key::kDiskCacheSize, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.05));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(key::kDiskCacheSize, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.1));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));
}

TEST(ExtensionListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  ExtensionListPolicyHandler handler(
      key::kExtensionInstallBlacklist, kTestPref, true);

  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("abcdefghijklmnopabcdefghijklmnop"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("*"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("invalid"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallBlacklist).empty());
}

TEST(ExtensionListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue policy;
  base::ListValue expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionListPolicyHandler handler(
      key::kExtensionInstallBlacklist, kTestPref, false);

  policy.Append(Value::CreateStringValue("abcdefghijklmnopabcdefghijklmnop"));
  expected.Append(Value::CreateStringValue("abcdefghijklmnopabcdefghijklmnop"));

  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, policy.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  policy.Append(Value::CreateStringValue("invalid"));
  policy_map.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, policy.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

TEST(ExtensionInstallForcelistPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  ExtensionInstallForcelistPolicyHandler handler(kTestPref);

  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Add an erroneous entry. This should generate an error, but the good
  // entry should still be translated successfully.
  list.AppendString("adfasdf;http://example.com");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(1U, errors.size());

  // Add an entry with bad URL, which should generate another error.
  list.AppendString("abcdefghijklmnopabcdefghijklmnop;nourl");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2U, errors.size());

  // Just an extension ID should also generate an error.
  list.AppendString("abcdefghijklmnopabcdefghijklmnop");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(3U, errors.size());
}

TEST(ExtensionInstallForcelistPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue policy;
  base::DictionaryValue expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionInstallForcelistPolicyHandler handler(kTestPref);

  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_FALSE(prefs.GetValue(kTestPref, &value));
  EXPECT_FALSE(value);

  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, policy.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  policy.AppendString("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  extensions::ExternalPolicyLoader::AddExtension(
      &expected, "abcdefghijklmnopabcdefghijklmnop", "http://example.com");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, policy.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  policy.AppendString("invalid");
  policy_map.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, policy.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

TEST(ExtensionURLPatternListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  ExtensionURLPatternListPolicyHandler handler(key::kExtensionInstallSources,
                                               kTestPref);

  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("http://*.google.com/*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("<all_urls>"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append(Value::CreateStringValue("invalid"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallSources).empty());

  // URLPattern syntax has a different way to express 'all urls'. Though '*'
  // would be compatible today, it would be brittle, so we disallow.
  list.Append(Value::CreateStringValue("*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kExtensionInstallSources).empty());
}

TEST(ExtensionURLPatternListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionURLPatternListPolicyHandler handler(key::kExtensionInstallSources,
                                               kTestPref);

  list.Append(Value::CreateStringValue("https://corp.monkey.net/*"));
  policy_map.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  ASSERT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&list, value));
}

}  // namespace policy
