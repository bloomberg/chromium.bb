// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/values.h"
#include "components/json_schema/json_schema_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

class JSONSchemaValidatorCPPTest : public testing::Test {
 public:
  JSONSchemaValidatorCPPTest() {}

};

TEST(JSONSchemaValidator, IsValidSchema) {
  std::string error;
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("\0", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("string", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("\"string\"", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("[]", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema("{}", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{ \"type\": 123 }", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{ \"type\": \"invalid\" }", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": []"  // Invalid properties type.
      "}", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"string\","
      "  \"maxLength\": -1"  // Must be >= 0.
      "}", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"string\","
      "  \"enum\": [ {} ]"  // "enum" dict values must contain "name".
      "}", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"string\","
      "  \"enum\": [ { \"name\": {} } ]"  // "enum" name must be a simple value.
      "}", &error));
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"array\","
      "  \"items\": [ 123 ],"  // "items" must contain a schema or schemas.
      "}", &error));
  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(
      "{ \"type\": \"object\" }", &error)) << error;
  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(
      "{ \"type\": [\"object\", \"array\"] }", &error)) << error;
  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": [\"object\", \"array\"],"
      "  \"properties\": {"
      "    \"string-property\": {"
      "      \"type\": \"string\","
      "      \"minLength\": 1,"
      "      \"maxLength\": 100,"
      "      \"title\": \"The String Policy\","
      "      \"description\": \"This policy controls the String widget.\""
      "    },"
      "    \"integer-property\": {"
      "      \"type\": \"number\","
      "      \"minimum\": 1000.0,"
      "      \"maximum\": 9999.0"
      "    },"
      "    \"enum-property\": {"
      "      \"type\": \"integer\","
      "      \"enum\": [0, 1, {\"name\": 10}, 100]"
      "    },"
      "    \"items-property\": {"
      "      \"type\": \"array\","
      "      \"items\": {"
      "        \"type\": \"string\""
      "      }"
      "    },"
      "    \"items-list-property\": {"
      "      \"type\": \"array\","
      "      \"items\": ["
      "        { \"type\": \"string\" },"
      "        { \"type\": \"integer\" }"
      "      ]"
      "    }"
      "  },"
      "  \"additionalProperties\": {"
      "    \"type\": \"any\""
      "  }"
      "}", &error)) << error;
  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"object\","
      "  \"patternProperties\": {"
      "    \".\": { \"type\": \"any\" },"
      "    \"foo\": { \"type\": \"any\" },"
      "    \"^foo$\": { \"type\": \"any\" },"
      "    \"foo+\": { \"type\": \"any\" },"
      "    \"foo?\": { \"type\": \"any\" },"
      "    \"fo{2,4}\": { \"type\": \"any\" },"
      "    \"(left)|(right)\": { \"type\": \"any\" }"
      "  }"
      "}", &error)) << error;
  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"object\","
      "  \"unknown attribute\": \"that should just be ignored\""
      "}",
      JSONSchemaValidator::OPTIONS_IGNORE_UNKNOWN_ATTRIBUTES,
      &error)) << error;
  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(
      "{"
      "  \"type\": \"object\","
      "  \"unknown attribute\": \"that will cause a failure\""
      "}", 0, &error)) << error;

  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(R"(
      {
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": 123
      })",
                                                  0, &error))
      << error;

  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(R"(
      {
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": [ 123 ]
      })",
                                                  0, &error))
      << error;

  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(R"(
      {
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": ["bar"]
      })",
                                                  0, &error))
      << error;

  EXPECT_FALSE(JSONSchemaValidator::IsValidSchema(R"(
      {
        "type": "object",
        "required": ["bar"]
      })",
                                                  0, &error))
      << error;

  EXPECT_TRUE(JSONSchemaValidator::IsValidSchema(R"(
      {
        "type": "object",
        "properties": {"foo": {"type": "number"}},
        "required": ["foo"]
      })",
                                                 0, &error))
      << error;
}
