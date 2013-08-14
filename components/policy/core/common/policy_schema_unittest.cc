// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_schema.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

#define SCHEMA_VERSION "\"$schema\":\"http://json-schema.org/draft-03/schema#\""
#define OBJECT_TYPE "\"type\":\"object\""

bool ParseFails(const std::string& content) {
  std::string error;
  scoped_ptr<PolicySchema> schema = PolicySchema::Parse(content, &error);
  EXPECT_TRUE(schema || !error.empty());
  return !schema;
}

}  // namespace

TEST(PolicySchemaTest, MinimalSchema) {
  EXPECT_FALSE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE
      "}"));
}

TEST(PolicySchemaTest, InvalidSchemas) {
  EXPECT_TRUE(ParseFails(""));
  EXPECT_TRUE(ParseFails("omg"));
  EXPECT_TRUE(ParseFails("\"omg\""));
  EXPECT_TRUE(ParseFails("123"));
  EXPECT_TRUE(ParseFails("[]"));
  EXPECT_TRUE(ParseFails("null"));
  EXPECT_TRUE(ParseFails("{}"));
  EXPECT_TRUE(ParseFails("{" SCHEMA_VERSION "}"));
  EXPECT_TRUE(ParseFails("{" OBJECT_TYPE "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"additionalProperties\": { \"type\":\"object\" }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"patternProperties\": { \"a+b*\": { \"type\": \"object\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": \"bogus\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": [\"string\", \"number\"] } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": \"any\" } }"
      "}"));
}

TEST(PolicySchemaTest, ValidSchema) {
  std::string error;
  scoped_ptr<PolicySchema> schema = PolicySchema::Parse(
      "{"
        SCHEMA_VERSION ","
        OBJECT_TYPE ","
        "\"properties\": {"
        "  \"Boolean\": { \"type\": \"boolean\" },"
        "  \"Integer\": { \"type\": \"integer\" },"
        "  \"Null\": { \"type\": \"null\" },"
        "  \"Number\": { \"type\": \"number\" },"
        "  \"String\": { \"type\": \"string\" },"
        "  \"Array\": {"
        "    \"type\": \"array\","
        "    \"items\": { \"type\": \"string\" }"
        "  },"
        "  \"ArrayOfObjects\": {"
        "    \"type\": \"array\","
        "    \"items\": {"
        "      \"type\": \"object\","
        "      \"properties\": {"
        "        \"one\": { \"type\": \"string\" },"
        "        \"two\": { \"type\": \"integer\" }"
        "      }"
        "    }"
        "  },"
        "  \"ArrayOfArray\": {"
        "    \"type\": \"array\","
        "    \"items\": {"
        "      \"type\": \"array\","
        "      \"items\": { \"type\": \"string\" }"
        "    }"
        "  },"
        "  \"Object\": {"
        "    \"type\": \"object\","
        "    \"properties\": {"
        "      \"one\": { \"type\": \"boolean\" },"
        "      \"two\": { \"type\": \"integer\" }"
        "    },"
        "    \"additionalProperties\": { \"type\": \"string\" }"
        "  }"
        "}"
      "}", &error);
  ASSERT_TRUE(schema) << error;

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema->type());
  EXPECT_FALSE(schema->GetSchemaForProperty("invalid"));

  const PolicySchema* sub = schema->GetSchemaForProperty("Boolean");
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, sub->type());

  sub = schema->GetSchemaForProperty("Integer");
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_INTEGER, sub->type());

  sub = schema->GetSchemaForProperty("Null");
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_NULL, sub->type());

  sub = schema->GetSchemaForProperty("Number");
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_DOUBLE, sub->type());
  sub = schema->GetSchemaForProperty("String");
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_STRING, sub->type());

  sub = schema->GetSchemaForProperty("Array");
  ASSERT_TRUE(sub);
  ASSERT_EQ(base::Value::TYPE_LIST, sub->type());
  sub = sub->GetSchemaForItems();
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_STRING, sub->type());

  sub = schema->GetSchemaForProperty("ArrayOfObjects");
  ASSERT_TRUE(sub);
  ASSERT_EQ(base::Value::TYPE_LIST, sub->type());
  sub = sub->GetSchemaForItems();
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, sub->type());
  const PolicySchema* subsub = sub->GetSchemaForProperty("one");
  ASSERT_TRUE(subsub);
  EXPECT_EQ(base::Value::TYPE_STRING, subsub->type());
  subsub = sub->GetSchemaForProperty("two");
  ASSERT_TRUE(subsub);
  EXPECT_EQ(base::Value::TYPE_INTEGER, subsub->type());
  subsub = sub->GetSchemaForProperty("invalid");
  EXPECT_FALSE(subsub);

  sub = schema->GetSchemaForProperty("ArrayOfArray");
  ASSERT_TRUE(sub);
  ASSERT_EQ(base::Value::TYPE_LIST, sub->type());
  sub = sub->GetSchemaForItems();
  ASSERT_TRUE(sub);
  ASSERT_EQ(base::Value::TYPE_LIST, sub->type());
  sub = sub->GetSchemaForItems();
  ASSERT_TRUE(sub);
  EXPECT_EQ(base::Value::TYPE_STRING, sub->type());

  sub = schema->GetSchemaForProperty("Object");
  ASSERT_TRUE(sub);
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, sub->type());
  subsub = sub->GetSchemaForProperty("one");
  ASSERT_TRUE(subsub);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, subsub->type());
  subsub = sub->GetSchemaForProperty("two");
  ASSERT_TRUE(subsub);
  EXPECT_EQ(base::Value::TYPE_INTEGER, subsub->type());
  subsub = sub->GetSchemaForProperty("undeclared");
  ASSERT_TRUE(subsub);
  EXPECT_EQ(base::Value::TYPE_STRING, subsub->type());
}

}  // namespace policy
