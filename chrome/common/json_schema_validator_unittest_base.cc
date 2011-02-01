// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/json_schema_validator_unittest_base.h"

#include <cfloat>
#include <cmath>
#include <limits>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_schema_validator.h"
#include "chrome/common/json_value_serializer.h"

namespace {

#define TEST_SOURCE base::StringPrintf("%s:%i", __FILE__, __LINE__)

Value* LoadValue(const std::string& filename) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("json_schema_validator").AppendASCII(filename);
  EXPECT_TRUE(file_util::PathExists(path));

  std::string error_message;
  JSONFileValueSerializer serializer(path);
  Value* result = serializer.Deserialize(NULL, &error_message);
  if (!result)
    ADD_FAILURE() << "Could not parse JSON: " << error_message;
  return result;
}

Value* LoadValue(const std::string& filename, Value::ValueType type) {
  scoped_ptr<Value> result(LoadValue(filename));
  if (!result.get())
    return NULL;
  if (!result->IsType(type)) {
    ADD_FAILURE() << "Expected type " << type << ", got: " << result->GetType();
    return NULL;
  }
  return result.release();
}

ListValue* LoadList(const std::string& filename) {
  return static_cast<ListValue*>(
      LoadValue(filename, Value::TYPE_LIST));
}

DictionaryValue* LoadDictionary(const std::string& filename) {
  return static_cast<DictionaryValue*>(
      LoadValue(filename, Value::TYPE_DICTIONARY));
}

}  // namespace


JSONSchemaValidatorTestBase::JSONSchemaValidatorTestBase(
    JSONSchemaValidatorTestBase::ValidatorType type)
    : type_(type) {
}

void JSONSchemaValidatorTestBase::RunTests() {
  TestComplex();
  TestStringPattern();
  TestEnum();
  TestChoices();
  TestExtends();
  TestObject();
  TestTypeReference();
  TestArrayTuple();
  TestArrayNonTuple();
  TestString();
  TestNumber();
  TestTypeClassifier();
  TestTypes();
}

void JSONSchemaValidatorTestBase::TestComplex() {
  scoped_ptr<DictionaryValue> schema(LoadDictionary("complex_schema.json"));
  scoped_ptr<ListValue> instance(LoadList("complex_instance.json"));

  ASSERT_TRUE(schema.get());
  ASSERT_TRUE(instance.get());

  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Remove(instance->GetSize() - 1, NULL);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Append(new DictionaryValue());
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "1",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "number", "object"));
  instance->Remove(instance->GetSize() - 1, NULL);

  DictionaryValue* item = NULL;
  ASSERT_TRUE(instance->GetDictionary(0, &item));
  item->SetString("url", "xxxxxxxxxxx");

  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL,
                 "0.url",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kStringMaxLength, "10"));
}

void JSONSchemaValidatorTestBase::TestStringPattern() {
  // Regex patterns not supported in CPP validator.
  if (type_ == CPP)
    return;

  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "string");
  schema->SetString("pattern", "foo+");

  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("foo")).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("foooooo")).get(),
              schema.get(), NULL);
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateStringValue("bar")).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kStringPattern, "foo+"));
}

void JSONSchemaValidatorTestBase::TestEnum() {
  scoped_ptr<DictionaryValue> schema(LoadDictionary("enum_schema.json"));

  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("foo")).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateBooleanValue(false)).get(),
              schema.get(), NULL);

  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateStringValue("42")).get(),
                 schema.get(), NULL, "", JSONSchemaValidator::kInvalidEnum);
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateNullValue()).get(),
                 schema.get(), NULL, "", JSONSchemaValidator::kInvalidEnum);
}

void JSONSchemaValidatorTestBase::TestChoices() {
  scoped_ptr<DictionaryValue> schema(LoadDictionary("choices_schema.json"));

  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateNullValue()).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
              schema.get(), NULL);

  scoped_ptr<DictionaryValue> instance(new DictionaryValue());
  instance->SetString("foo", "bar");
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateStringValue("foo")).get(),
                 schema.get(), NULL, "", JSONSchemaValidator::kInvalidChoice);
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(new ListValue()).get(),
                 schema.get(), NULL, "", JSONSchemaValidator::kInvalidChoice);

  instance->SetInteger("foo", 42);
  ExpectNotValid(TEST_SOURCE, instance.get(),
                 schema.get(), NULL, "", JSONSchemaValidator::kInvalidChoice);
}

void JSONSchemaValidatorTestBase::TestExtends() {
  // TODO(aa): JS only
}

void JSONSchemaValidatorTestBase::TestObject() {
  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "object");
  schema->SetString("properties.foo.type", "string");
  schema->SetString("properties.bar.type", "integer");

  scoped_ptr<DictionaryValue> instance(new DictionaryValue());
  instance->SetString("foo", "foo");
  instance->SetInteger("bar", 42);

  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  instance->SetBoolean("extra", true);
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL,
                 "extra", JSONSchemaValidator::kUnexpectedProperty);

  instance->Remove("extra", NULL);
  instance->Remove("bar", NULL);
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "bar",
                 JSONSchemaValidator::kObjectPropertyIsRequired);

  instance->SetString("bar", "42");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "bar",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "integer", "string"));

  DictionaryValue* additional_properties = new DictionaryValue();
  additional_properties->SetString("type", "any");
  schema->Set("additionalProperties", additional_properties);

  instance->SetInteger("bar", 42);
  instance->SetBoolean("extra", true);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  instance->SetString("extra", "foo");
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  additional_properties->SetString("type", "boolean");
  instance->SetBoolean("extra", true);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  instance->SetString("extra", "foo");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL,
                 "extra", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "boolean", "string"));

  DictionaryValue* properties = NULL;
  DictionaryValue* bar_property = NULL;
  ASSERT_TRUE(schema->GetDictionary("properties", &properties));
  ASSERT_TRUE(properties->GetDictionary("bar", &bar_property));

  bar_property->SetBoolean("optional", true);
  instance->Remove("extra", NULL);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Remove("bar", NULL);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Set("bar", Value::CreateNullValue());
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL,
                 "bar", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "integer", "null"));
  instance->SetString("bar", "42");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL,
                 "bar", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "integer", "string"));
}

void JSONSchemaValidatorTestBase::TestTypeReference() {
  scoped_ptr<ListValue> types(LoadList("reference_types.json"));
  ASSERT_TRUE(types.get());

  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "object");
  schema->SetString("properties.foo.type", "string");
  schema->SetString("properties.bar.$ref", "Max10Int");
  schema->SetString("properties.baz.$ref", "MinLengthString");

  scoped_ptr<DictionaryValue> schema_inline(new DictionaryValue());
  schema_inline->SetString("type", "object");
  schema_inline->SetString("properties.foo.type", "string");
  schema_inline->SetString("properties.bar.id", "NegativeInt");
  schema_inline->SetString("properties.bar.type", "integer");
  schema_inline->SetInteger("properties.bar.maximum", 0);
  schema_inline->SetString("properties.baz.$ref", "NegativeInt");

  scoped_ptr<DictionaryValue> instance(new DictionaryValue());
  instance->SetString("foo", "foo");
  instance->SetInteger("bar", 4);
  instance->SetString("baz", "ab");

  scoped_ptr<DictionaryValue> instance_inline(new DictionaryValue());
  instance_inline->SetString("foo", "foo");
  instance_inline->SetInteger("bar", -4);
  instance_inline->SetInteger("baz", -2);

  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), types.get());
  ExpectValid(TEST_SOURCE, instance_inline.get(), schema_inline.get(), NULL);

  // Validation failure, but successful schema reference.
  instance->SetString("baz", "a");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), types.get(),
                 "baz", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kStringMinLength, "2"));

  instance_inline->SetInteger("bar", 20);
  ExpectNotValid(TEST_SOURCE, instance_inline.get(), schema_inline.get(), NULL,
                 "bar", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kNumberMaximum, "0"));

  // Remove MinLengthString type.
  types->Remove(types->GetSize() - 1, NULL);
  instance->SetString("baz", "ab");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), types.get(),
                 "bar", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kUnknownTypeReference,
                     "Max10Int"));

  // Remove internal type "NegativeInt".
  schema_inline->Remove("properties.bar", NULL);
  instance_inline->Remove("bar", NULL);
  ExpectNotValid(TEST_SOURCE, instance_inline.get(), schema_inline.get(), NULL,
                 "baz", JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kUnknownTypeReference,
                     "NegativeInt"));
}

void JSONSchemaValidatorTestBase::TestArrayTuple() {
  scoped_ptr<DictionaryValue> schema(LoadDictionary("array_tuple_schema.json"));
  ASSERT_TRUE(schema.get());

  scoped_ptr<ListValue> instance(new ListValue());
  instance->Append(Value::CreateStringValue("42"));
  instance->Append(Value::CreateIntegerValue(42));

  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  instance->Append(Value::CreateStringValue("anything"));
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kArrayMaxItems, "2"));

  instance->Remove(1, NULL);
  instance->Remove(1, NULL);
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "1",
                 JSONSchemaValidator::kArrayItemRequired);

  instance->Set(0, Value::CreateIntegerValue(42));
  instance->Append(Value::CreateIntegerValue(42));
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "0",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "string", "integer"));

  DictionaryValue* additional_properties = new DictionaryValue();
  additional_properties->SetString("type", "any");
  schema->Set("additionalProperties", additional_properties);
  instance->Set(0, Value::CreateStringValue("42"));
  instance->Append(Value::CreateStringValue("anything"));
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Set(2, new ListValue());
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  additional_properties->SetString("type", "boolean");
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "2",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "boolean", "array"));
  instance->Set(2, Value::CreateBooleanValue(false));
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  ListValue* items_schema = NULL;
  DictionaryValue* item0_schema = NULL;
  ASSERT_TRUE(schema->GetList("items", &items_schema));
  ASSERT_TRUE(items_schema->GetDictionary(0, &item0_schema));
  item0_schema->SetBoolean("optional", true);
  instance->Remove(2, NULL);
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  // TODO(aa): I think this is inconsistent with the handling of NULL+optional
  // for objects.
  instance->Set(0, Value::CreateNullValue());
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Set(0, Value::CreateIntegerValue(42));
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "0",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "string", "integer"));
}

void JSONSchemaValidatorTestBase::TestArrayNonTuple() {
  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "array");
  schema->SetString("items.type", "string");
  schema->SetInteger("minItems", 2);
  schema->SetInteger("maxItems", 3);

  scoped_ptr<ListValue> instance(new ListValue());
  instance->Append(Value::CreateStringValue("x"));
  instance->Append(Value::CreateStringValue("x"));

  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);
  instance->Append(Value::CreateStringValue("x"));
  ExpectValid(TEST_SOURCE, instance.get(), schema.get(), NULL);

  instance->Append(Value::CreateStringValue("x"));
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kArrayMaxItems, "3"));
  instance->Remove(1, NULL);
  instance->Remove(1, NULL);
  instance->Remove(1, NULL);
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kArrayMinItems, "2"));

  instance->Remove(1, NULL);
  instance->Append(Value::CreateIntegerValue(42));
  ExpectNotValid(TEST_SOURCE, instance.get(), schema.get(), NULL, "1",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "string", "integer"));
}

void JSONSchemaValidatorTestBase::TestString() {
  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "string");
  schema->SetInteger("minLength", 1);
  schema->SetInteger("maxLength", 10);

  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("x")).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("xxxxxxxxxx")).get(),
              schema.get(), NULL);

  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateStringValue("")).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kStringMinLength, "1"));
  ExpectNotValid(
      TEST_SOURCE,
      scoped_ptr<Value>(Value::CreateStringValue("xxxxxxxxxxx")).get(),
      schema.get(), NULL, "",
      JSONSchemaValidator::FormatErrorMessage(
          JSONSchemaValidator::kStringMaxLength, "10"));

}

void JSONSchemaValidatorTestBase::TestNumber() {
  scoped_ptr<DictionaryValue> schema(new DictionaryValue());
  schema->SetString("type", "number");
  schema->SetInteger("minimum", 1);
  schema->SetInteger("maximum", 100);
  schema->SetInteger("maxDecimal", 2);

  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(1)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(50)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(100)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateDoubleValue(88.88)).get(),
              schema.get(), NULL);

  ExpectNotValid(
      TEST_SOURCE,
      scoped_ptr<Value>(Value::CreateDoubleValue(0.5)).get(),
      schema.get(), NULL, "",
      JSONSchemaValidator::FormatErrorMessage(
          JSONSchemaValidator::kNumberMinimum, "1"));
  ExpectNotValid(
      TEST_SOURCE,
      scoped_ptr<Value>(Value::CreateDoubleValue(100.1)).get(),
      schema.get(), NULL, "",
      JSONSchemaValidator::FormatErrorMessage(
          JSONSchemaValidator::kNumberMaximum, "100"));
}

void JSONSchemaValidatorTestBase::TestTypeClassifier() {
  EXPECT_EQ("boolean", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateBooleanValue(true)).get()));
  EXPECT_EQ("boolean", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateBooleanValue(false)).get()));

  // It doesn't matter whether the C++ type is 'integer' or 'real'. If the
  // number is integral and within the representable range of integers in
  // double, it's classified as 'integer'.
  EXPECT_EQ("integer", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateIntegerValue(42)).get()));
  EXPECT_EQ("integer", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateIntegerValue(0)).get()));
  EXPECT_EQ("integer", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateDoubleValue(42)).get()));
  EXPECT_EQ("integer", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(
          Value::CreateDoubleValue(pow(2.0, DBL_MANT_DIG))).get()));
  EXPECT_EQ("integer", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(
          Value::CreateDoubleValue(pow(-2.0, DBL_MANT_DIG))).get()));

  // "number" is only used for non-integral numbers, or numbers beyond what
  // double can accurately represent.
  EXPECT_EQ("number", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateDoubleValue(88.8)).get()));
  EXPECT_EQ("number", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateDoubleValue(
          pow(2.0, DBL_MANT_DIG) * 2)).get()));
  EXPECT_EQ("number", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateDoubleValue(
          pow(-2.0, DBL_MANT_DIG) * 2)).get()));

  EXPECT_EQ("string", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateStringValue("foo")).get()));
  EXPECT_EQ("array", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(new ListValue()).get()));
  EXPECT_EQ("object", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(new DictionaryValue()).get()));
  EXPECT_EQ("null", JSONSchemaValidator::GetJSONSchemaType(
      scoped_ptr<Value>(Value::CreateNullValue()).get()));
}

void JSONSchemaValidatorTestBase::TestTypes() {
  scoped_ptr<DictionaryValue> schema(new DictionaryValue());

  // valid
  schema->SetString("type", "object");
  ExpectValid(TEST_SOURCE, scoped_ptr<Value>(new DictionaryValue()).get(),
              schema.get(), NULL);

  schema->SetString("type", "array");
  ExpectValid(TEST_SOURCE, scoped_ptr<Value>(new ListValue()).get(),
              schema.get(), NULL);

  schema->SetString("type", "string");
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateStringValue("foobar")).get(),
              schema.get(), NULL);

  schema->SetString("type", "number");
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateDoubleValue(88.8)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateDoubleValue(42)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(0)).get(),
              schema.get(), NULL);

  schema->SetString("type", "integer");
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateDoubleValue(42)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateIntegerValue(0)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(
                  Value::CreateDoubleValue(pow(2.0, DBL_MANT_DIG))).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(
                  Value::CreateDoubleValue(pow(-2.0, DBL_MANT_DIG))).get(),
              schema.get(), NULL);

  schema->SetString("type", "boolean");
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateBooleanValue(false)).get(),
              schema.get(), NULL);
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateBooleanValue(true)).get(),
              schema.get(), NULL);

  schema->SetString("type", "null");
  ExpectValid(TEST_SOURCE,
              scoped_ptr<Value>(Value::CreateNullValue()).get(),
              schema.get(), NULL);

  // not valid
  schema->SetString("type", "object");
  ExpectNotValid(TEST_SOURCE, scoped_ptr<Value>(new ListValue()).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "object", "array"));

  schema->SetString("type", "object");
  ExpectNotValid(TEST_SOURCE, scoped_ptr<Value>(Value::CreateNullValue()).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "object", "null"));

  schema->SetString("type", "array");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "array", "integer"));

  schema->SetString("type", "string");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateIntegerValue(42)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "string", "integer"));

  schema->SetString("type", "number");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateStringValue("42")).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "number", "string"));

  schema->SetString("type", "integer");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateDoubleValue(88.8)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "integer", "number"));

  schema->SetString("type", "integer");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateDoubleValue(88.8)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "integer", "number"));

  schema->SetString("type", "boolean");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateIntegerValue(1)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "boolean", "integer"));

  schema->SetString("type", "null");
  ExpectNotValid(TEST_SOURCE,
                 scoped_ptr<Value>(Value::CreateBooleanValue(false)).get(),
                 schema.get(), NULL, "",
                 JSONSchemaValidator::FormatErrorMessage(
                     JSONSchemaValidator::kInvalidType, "null", "boolean"));
}
