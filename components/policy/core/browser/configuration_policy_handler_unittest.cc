// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

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
