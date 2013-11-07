// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema.h"

#include "components/policy/core/common/schema_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

#define OBJECT_TYPE "\"type\":\"object\""

bool ParseFails(const std::string& content) {
  std::string error;
  scoped_ptr<SchemaOwner> schema = SchemaOwner::Parse(content, &error);
  if (schema)
    EXPECT_TRUE(schema->schema().valid());
  else
    EXPECT_FALSE(error.empty());
  return !schema;
}

}  // namespace

TEST(SchemaTest, MinimalSchema) {
  EXPECT_FALSE(ParseFails(
      "{"
        OBJECT_TYPE
      "}"));
}

TEST(SchemaTest, InvalidSchemas) {
  EXPECT_TRUE(ParseFails(""));
  EXPECT_TRUE(ParseFails("omg"));
  EXPECT_TRUE(ParseFails("\"omg\""));
  EXPECT_TRUE(ParseFails("123"));
  EXPECT_TRUE(ParseFails("[]"));
  EXPECT_TRUE(ParseFails("null"));
  EXPECT_TRUE(ParseFails("{}"));

  EXPECT_TRUE(ParseFails(
      "{"
        OBJECT_TYPE ","
        "\"additionalProperties\": { \"type\":\"object\" }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        OBJECT_TYPE ","
        "\"patternProperties\": { \"a+b*\": { \"type\": \"object\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": \"bogus\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": [\"string\", \"number\"] } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
        OBJECT_TYPE ","
        "\"properties\": { \"Policy\": { \"type\": \"any\" } }"
      "}"));
}

TEST(SchemaTest, ValidSchema) {
  std::string error;
  scoped_ptr<SchemaOwner> policy_schema = SchemaOwner::Parse(
      "{"
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
  ASSERT_TRUE(policy_schema) << error;
  ASSERT_TRUE(policy_schema->schema().valid());

  Schema schema = policy_schema->schema();
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());
  EXPECT_FALSE(schema.GetProperty("invalid").valid());

  Schema sub = schema.GetProperty("Boolean");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, sub.type());

  sub = schema.GetProperty("Integer");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_INTEGER, sub.type());

  sub = schema.GetProperty("Null");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_NULL, sub.type());

  sub = schema.GetProperty("Number");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_DOUBLE, sub.type());

  sub = schema.GetProperty("String");
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, sub.type());

  sub = schema.GetProperty("Array");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, sub.type());

  sub = schema.GetProperty("ArrayOfObjects");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, sub.type());
  Schema subsub = sub.GetProperty("one");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, subsub.type());
  subsub = sub.GetProperty("two");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::TYPE_INTEGER, subsub.type());
  subsub = sub.GetProperty("invalid");
  EXPECT_FALSE(subsub.valid());

  sub = schema.GetProperty("ArrayOfArray");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
  sub = sub.GetItems();
  ASSERT_TRUE(sub.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, sub.type());

  sub = schema.GetProperty("Object");
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, sub.type());
  subsub = sub.GetProperty("one");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, subsub.type());
  subsub = sub.GetProperty("two");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::TYPE_INTEGER, subsub.type());
  subsub = sub.GetProperty("undeclared");
  ASSERT_TRUE(subsub.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, subsub.type());

  struct {
    const char* expected_key;
    base::Value::Type expected_type;
  } kExpectedProperties[] = {
    { "Array",            base::Value::TYPE_LIST },
    { "ArrayOfArray",     base::Value::TYPE_LIST },
    { "ArrayOfObjects",   base::Value::TYPE_LIST },
    { "Boolean",          base::Value::TYPE_BOOLEAN },
    { "Integer",          base::Value::TYPE_INTEGER },
    { "Null",             base::Value::TYPE_NULL },
    { "Number",           base::Value::TYPE_DOUBLE },
    { "Object",           base::Value::TYPE_DICTIONARY },
    { "String",           base::Value::TYPE_STRING },
  };
  Schema::Iterator it = schema.GetPropertiesIterator();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExpectedProperties); ++i) {
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ(kExpectedProperties[i].expected_key, it.key());
    ASSERT_TRUE(it.schema().valid());
    EXPECT_EQ(kExpectedProperties[i].expected_type, it.schema().type());
    it.Advance();
  }
  EXPECT_TRUE(it.IsAtEnd());
}

TEST(SchemaTest, Lookups) {
  std::string error;
  scoped_ptr<SchemaOwner> policy_schema = SchemaOwner::Parse(
      "{"
        OBJECT_TYPE
      "}", &error);
  ASSERT_TRUE(policy_schema) << error;
  Schema schema = policy_schema->schema();
  ASSERT_TRUE(schema.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  // This empty schema should never find named properties.
  EXPECT_FALSE(schema.GetKnownProperty("").valid());
  EXPECT_FALSE(schema.GetKnownProperty("xyz").valid());
  EXPECT_TRUE(schema.GetPropertiesIterator().IsAtEnd());

  policy_schema = SchemaOwner::Parse(
      "{"
        OBJECT_TYPE ","
        "\"properties\": {"
        "  \"Boolean\": { \"type\": \"boolean\" }"
        "}"
      "}", &error);
  ASSERT_TRUE(policy_schema) << error;
  schema = policy_schema->schema();
  ASSERT_TRUE(schema.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  EXPECT_FALSE(schema.GetKnownProperty("").valid());
  EXPECT_FALSE(schema.GetKnownProperty("xyz").valid());
  EXPECT_TRUE(schema.GetKnownProperty("Boolean").valid());

  policy_schema = SchemaOwner::Parse(
      "{"
        OBJECT_TYPE ","
        "\"properties\": {"
          "  \"bb\" : { \"type\": \"null\" },"
          "  \"aa\" : { \"type\": \"boolean\" },"
          "  \"abab\" : { \"type\": \"string\" },"
          "  \"ab\" : { \"type\": \"number\" },"
          "  \"aba\" : { \"type\": \"integer\" }"
        "}"
      "}", &error);
  ASSERT_TRUE(policy_schema) << error;
  schema = policy_schema->schema();
  ASSERT_TRUE(schema.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  EXPECT_FALSE(schema.GetKnownProperty("").valid());
  EXPECT_FALSE(schema.GetKnownProperty("xyz").valid());

  struct {
    const char* expected_key;
    base::Value::Type expected_type;
  } kExpectedKeys[] = {
    { "aa",     base::Value::TYPE_BOOLEAN },
    { "ab",     base::Value::TYPE_DOUBLE },
    { "aba",    base::Value::TYPE_INTEGER },
    { "abab",   base::Value::TYPE_STRING },
    { "bb",     base::Value::TYPE_NULL },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExpectedKeys); ++i) {
    Schema sub = schema.GetKnownProperty(kExpectedKeys[i].expected_key);
    ASSERT_TRUE(sub.valid());
    EXPECT_EQ(kExpectedKeys[i].expected_type, sub.type());
  }
}

TEST(SchemaTest, Wrap) {
  const internal::SchemaNode kSchemas[] = {
    { base::Value::TYPE_DICTIONARY,   0 },    // 0: root node
    { base::Value::TYPE_BOOLEAN,      -1 },   // 1
    { base::Value::TYPE_INTEGER,      -1 },   // 2
    { base::Value::TYPE_DOUBLE,       -1 },   // 3
    { base::Value::TYPE_STRING,       -1 },   // 4
    { base::Value::TYPE_LIST,         4 },    // 5: list of strings.
    { base::Value::TYPE_LIST,         5 },    // 6: list of lists of strings.
  };

  const internal::PropertyNode kPropertyNodes[] = {
    { "Boolean",  1 },
    { "Integer",  2 },
    { "Number",   3 },
    { "String",   4 },
    { "List",     5 },
  };

  const internal::PropertiesNode kProperties[] = {
    // Properties 0 to 5 (exclusive) are known, from kPropertyNodes.
    // SchemaNode offset 6 is for additionalProperties (list of lists).
    { 0, 5, 6 },
  };

  const internal::SchemaData kData = {
    kSchemas,
    kPropertyNodes,
    kProperties,
  };

  scoped_ptr<SchemaOwner> policy_schema = SchemaOwner::Wrap(&kData);
  ASSERT_TRUE(policy_schema);
  Schema schema = policy_schema->schema();
  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  struct {
    const char* key;
    base::Value::Type type;
  } kExpectedProperties[] = {
    { "Boolean", base::Value::TYPE_BOOLEAN },
    { "Integer", base::Value::TYPE_INTEGER },
    { "Number", base::Value::TYPE_DOUBLE },
    { "String", base::Value::TYPE_STRING },
    { "List", base::Value::TYPE_LIST },
  };

  Schema::Iterator it = schema.GetPropertiesIterator();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExpectedProperties); ++i) {
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ(kExpectedProperties[i].key, it.key());
    Schema sub = it.schema();
    ASSERT_TRUE(sub.valid());
    EXPECT_EQ(kExpectedProperties[i].type, sub.type());

    if (sub.type() == base::Value::TYPE_LIST) {
      ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
      Schema items = sub.GetItems();
      ASSERT_TRUE(items.valid());
      EXPECT_EQ(base::Value::TYPE_STRING, items.type());
    }

    it.Advance();
  }
  EXPECT_TRUE(it.IsAtEnd());

  Schema sub = schema.GetAdditionalProperties();
  ASSERT_TRUE(sub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, sub.type());
  Schema subsub = sub.GetItems();
  ASSERT_TRUE(subsub.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, subsub.type());
  Schema subsubsub = subsub.GetItems();
  ASSERT_TRUE(subsubsub.valid());
  ASSERT_EQ(base::Value::TYPE_STRING, subsubsub.type());
}

}  // namespace policy
