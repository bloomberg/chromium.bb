// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

StringToIntEnumListPolicyHandler::MappingEntry kTestTypeMap[] = {
  { "one", 1 },
  { "two", 2 },
};

const char kTestPolicy[] = "unit_test.test_policy";
const char kTestPref[] = "unit_test.test_pref";

class SimpleSchemaValidatingPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  SimpleSchemaValidatingPolicyHandler(const Schema& schema,
                                      SchemaOnErrorStrategy strategy)
      : SchemaValidatingPolicyHandler("PolicyForTesting", schema, strategy) {}
  virtual ~SimpleSchemaValidatingPolicyHandler() {}

  virtual void ApplyPolicySettings(const policy::PolicyMap&,
                                   PrefValueMap*) OVERRIDE {
  }

  bool CheckAndGetValueForTest(const PolicyMap& policies,
                               scoped_ptr<base::Value>* value) {
    return SchemaValidatingPolicyHandler::CheckAndGetValue(
        policies, NULL, value);
  }
};

}  // namespace

TEST(StringToIntEnumListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  StringToIntEnumListPolicyHandler handler(
      kTestPolicy,
      kTestPref,
      kTestTypeMap,
      kTestTypeMap + arraysize(kTestTypeMap));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("one");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("invalid");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(kTestPolicy).empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("no list"), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(kTestPolicy).empty());
}

TEST(StringToIntEnumListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  base::ListValue expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value;
  StringToIntEnumListPolicyHandler handler(
      kTestPolicy,
      kTestPref,
      kTestTypeMap,
      kTestTypeMap + arraysize(kTestTypeMap));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  list.AppendString("two");
  expected.AppendInteger(2);
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));

  list.AppendString("invalid");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
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
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy,
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
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
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
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(5));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(10));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
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
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateIntegerValue(5));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
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
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
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
      kTestPolicy, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(15), NULL);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
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
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.05));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.1));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(-5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
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
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.0));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(5), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.05));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateIntegerValue(10), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected.reset(base::Value::CreateDoubleValue(0.1));
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value::Equals(expected.get(), value));
}

TEST(SchemaValidatingPolicyHandlerTest, CheckAndGetValue) {
  std::string error;
  static const char kSchemaJson[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"OneToThree\": {"
      "      \"type\": \"integer\","
      "      \"minimum\": 1,"
      "      \"maximum\": 3"
      "    },"
      "    \"Colors\": {"
      "      \"type\": \"string\","
      "      \"enum\": [ \"Red\", \"Green\", \"Blue\" ]"
      "    }"
      "  }"
      "}";
  Schema schema = Schema::Parse(kSchemaJson, &error);
  ASSERT_TRUE(schema.valid()) << error;

  static const char kPolicyMapJson[] =
      "{"
      "  \"PolicyForTesting\": {"
      "    \"OneToThree\": 2,"
      "    \"Colors\": \"White\""
      "  }"
      "}";
  scoped_ptr<base::Value> policy_map_value(base::JSONReader::ReadAndReturnError(
      kPolicyMapJson, base::JSON_PARSE_RFC, NULL, &error));
  ASSERT_TRUE(policy_map_value) << error;

  const base::DictionaryValue* policy_map_dict = NULL;
  ASSERT_TRUE(policy_map_value->GetAsDictionary(&policy_map_dict));

  PolicyMap policy_map;
  policy_map.LoadFrom(
      policy_map_dict, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);

  SimpleSchemaValidatingPolicyHandler handler(schema, SCHEMA_ALLOW_INVALID);
  scoped_ptr<base::Value> output_value;
  ASSERT_TRUE(handler.CheckAndGetValueForTest(policy_map, &output_value));
  ASSERT_TRUE(output_value);

  base::DictionaryValue* dict = NULL;
  ASSERT_TRUE(output_value->GetAsDictionary(&dict));

  // Test that CheckAndGetValue() actually dropped invalid properties.
  int int_value = -1;
  EXPECT_TRUE(dict->GetInteger("OneToThree", &int_value));
  EXPECT_EQ(2, int_value);
  EXPECT_FALSE(dict->HasKey("Colors"));
}

}  // namespace policy
