// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema.h"

#include "components/policy/core/common/schema_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kTestSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"Boolean\": { \"type\": \"boolean\" },"
    "    \"Integer\": { \"type\": \"integer\" },"
    "    \"Null\": { \"type\": \"null\" },"
    "    \"Number\": { \"type\": \"number\" },"
    "    \"String\": { \"type\": \"string\" },"
    "    \"Array\": {"
    "      \"type\": \"array\","
    "      \"items\": { \"type\": \"string\" }"
    "    },"
    "    \"ArrayOfObjects\": {"
    "      \"type\": \"array\","
    "      \"items\": {"
    "        \"type\": \"object\","
    "        \"properties\": {"
    "          \"one\": { \"type\": \"string\" },"
    "          \"two\": { \"type\": \"integer\" }"
    "        }"
    "      }"
    "    },"
    "    \"ArrayOfArray\": {"
    "      \"type\": \"array\","
    "      \"items\": {"
    "        \"type\": \"array\","
    "        \"items\": { \"type\": \"string\" }"
    "      }"
    "    },"
    "    \"Object\": {"
    "      \"type\": \"object\","
    "      \"properties\": {"
    "        \"one\": { \"type\": \"boolean\" },"
    "        \"two\": { \"type\": \"integer\" }"
    "      },"
    "      \"additionalProperties\": { \"type\": \"string\" }"
    "    }"
    "  }"
    "}";

bool ParseFails(const std::string& content) {
  std::string error;
  Schema schema = Schema::Parse(content, &error);
  if (schema.valid())
    return false;
  EXPECT_FALSE(error.empty());
  return true;
}

}  // namespace

TEST(SchemaTest, MinimalSchema) {
  EXPECT_FALSE(ParseFails("{ \"type\": \"object\" }"));
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
      "  \"type\": \"object\","
        "\"additionalProperties\": { \"type\":\"object\" }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"patternProperties\": { \"a+b*\": { \"type\": \"object\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": { \"Policy\": { \"type\": \"bogus\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": { \"Policy\": { \"type\": [\"string\", \"number\"] } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": { \"Policy\": { \"type\": \"any\" } }"
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": { \"Policy\": 123 }"
      "}"));
}

TEST(SchemaTest, Ownership) {
  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"sub\": {"
      "      \"type\": \"object\","
      "      \"properties\": {"
      "        \"subsub\": { \"type\": \"string\" }"
      "      }"
      "    }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  schema = schema.GetKnownProperty("sub");
  ASSERT_TRUE(schema.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  {
    Schema::Iterator it = schema.GetPropertiesIterator();
    ASSERT_FALSE(it.IsAtEnd());
    EXPECT_STREQ("subsub", it.key());

    schema = it.schema();
    it.Advance();
    EXPECT_TRUE(it.IsAtEnd());
  }

  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, schema.type());

  // This test shouldn't leak nor use invalid memory.
}

TEST(SchemaTest, ValidSchema) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

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

  Schema schema = Schema::Parse("{ \"type\": \"object\" }", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  // This empty schema should never find named properties.
  EXPECT_FALSE(schema.GetKnownProperty("").valid());
  EXPECT_FALSE(schema.GetKnownProperty("xyz").valid());
  EXPECT_TRUE(schema.GetPropertiesIterator().IsAtEnd());

  schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"Boolean\": { \"type\": \"boolean\" }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  EXPECT_FALSE(schema.GetKnownProperty("").valid());
  EXPECT_FALSE(schema.GetKnownProperty("xyz").valid());
  EXPECT_TRUE(schema.GetKnownProperty("Boolean").valid());

  schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"bb\" : { \"type\": \"null\" },"
      "    \"aa\" : { \"type\": \"boolean\" },"
      "    \"abab\" : { \"type\": \"string\" },"
      "    \"ab\" : { \"type\": \"number\" },"
      "    \"aba\" : { \"type\": \"integer\" }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
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

  Schema schema = Schema::Wrap(&kData);
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

TEST(SchemaTest, Validate) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  base::DictionaryValue bundle;
  EXPECT_TRUE(schema.Validate(bundle));

  // Wrong type, expected integer.
  bundle.SetBoolean("Integer", true);
  EXPECT_FALSE(schema.Validate(bundle));

  // Wrong type, expected list of strings.
  {
    bundle.Clear();
    base::ListValue list;
    list.AppendInteger(1);
    bundle.Set("Array", list.DeepCopy());
    EXPECT_FALSE(schema.Validate(bundle));
  }

  // Wrong type in a sub-object.
  {
    bundle.Clear();
    base::DictionaryValue dict;
    dict.SetString("one", "one");
    bundle.Set("Object", dict.DeepCopy());
    EXPECT_FALSE(schema.Validate(bundle));
  }

  // Unknown name.
  bundle.Clear();
  bundle.SetBoolean("Unknown", true);
  EXPECT_FALSE(schema.Validate(bundle));

  // All of these will be valid.
  bundle.Clear();
  bundle.SetBoolean("Boolean", true);
  bundle.SetInteger("Integer", 123);
  bundle.Set("Null", base::Value::CreateNullValue());
  bundle.Set("Number", base::Value::CreateDoubleValue(3.14));
  bundle.SetString("String", "omg");

  {
    base::ListValue list;
    list.AppendString("a string");
    list.AppendString("another string");
    bundle.Set("Array", list.DeepCopy());
  }

  {
    base::DictionaryValue dict;
    dict.SetString("one", "string");
    dict.SetInteger("two", 2);
    base::ListValue list;
    list.Append(dict.DeepCopy());
    list.Append(dict.DeepCopy());
    bundle.Set("ArrayOfObjects", list.DeepCopy());
  }

  {
    base::ListValue list;
    list.AppendString("a string");
    list.AppendString("another string");
    base::ListValue listlist;
    listlist.Append(list.DeepCopy());
    listlist.Append(list.DeepCopy());
    bundle.Set("ArrayOfArray", listlist.DeepCopy());
  }

  {
    base::DictionaryValue dict;
    dict.SetBoolean("one", true);
    dict.SetInteger("two", 2);
    dict.SetString("additionally", "a string");
    dict.SetString("and also", "another string");
    bundle.Set("Object", dict.DeepCopy());
  }

  EXPECT_TRUE(schema.Validate(bundle));

  bundle.SetString("boom", "bang");
  EXPECT_FALSE(schema.Validate(bundle));

}
TEST(SchemaTest, InvalidReferences) {
  // References to undeclared schemas fail.
  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"name\": { \"$ref\": \"undeclared\" }"
      "  }"
      "}"));

  // Can't refer to self.
  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"name\": {"
      "      \"id\": \"self\","
      "      \"$ref\": \"self\""
      "    }"
      "  }"
      "}"));

  // Duplicated IDs are invalid.
  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"name\": {"
      "      \"id\": \"x\","
      "      \"type\": \"string\""
      "    },"
      "    \"another\": {"
      "      \"id\": \"x\","
      "      \"type\": \"string\""
      "    }"
      "  }"
      "}"));

  // Main object can't be a reference.
  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"id\": \"main\","
      "  \"$ref\": \"main\""
      "}"));

  EXPECT_TRUE(ParseFails(
      "{"
      "  \"type\": \"object\","
      "  \"$ref\": \"main\""
      "}"));
}

TEST(SchemaTest, RecursiveReferences) {
  // Verifies that references can go to a parent schema, to define a
  // recursive type.
  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"bookmarks\": {"
      "      \"type\": \"array\","
      "      \"id\": \"ListOfBookmarks\","
      "      \"items\": {"
      "        \"type\": \"object\","
      "        \"properties\": {"
      "          \"name\": { \"type\": \"string\" },"
      "          \"url\": { \"type\": \"string\" },"
      "          \"children\": { \"$ref\": \"ListOfBookmarks\" }"
      "        }"
      "      }"
      "    }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  Schema parent = schema.GetKnownProperty("bookmarks");
  ASSERT_TRUE(parent.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, parent.type());

  // Check the recursive type a number of times.
  for (int i = 0; i < 10; ++i) {
    Schema items = parent.GetItems();
    ASSERT_TRUE(items.valid());
    ASSERT_EQ(base::Value::TYPE_DICTIONARY, items.type());

    Schema prop = items.GetKnownProperty("name");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::TYPE_STRING, prop.type());

    prop = items.GetKnownProperty("url");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::TYPE_STRING, prop.type());

    prop = items.GetKnownProperty("children");
    ASSERT_TRUE(prop.valid());
    ASSERT_EQ(base::Value::TYPE_LIST, prop.type());

    parent = prop;
  }
}

TEST(SchemaTest, UnorderedReferences) {
  // Verifies that references and IDs can come in any order.
  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"a\": { \"$ref\": \"shared\" },"
      "    \"b\": { \"$ref\": \"shared\" },"
      "    \"c\": { \"$ref\": \"shared\" },"
      "    \"d\": { \"$ref\": \"shared\" },"
      "    \"e\": {"
      "      \"type\": \"boolean\","
      "      \"id\": \"shared\""
      "    },"
      "    \"f\": { \"$ref\": \"shared\" },"
      "    \"g\": { \"$ref\": \"shared\" },"
      "    \"h\": { \"$ref\": \"shared\" },"
      "    \"i\": { \"$ref\": \"shared\" }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  for (char c = 'a'; c <= 'i'; ++c) {
    Schema sub = schema.GetKnownProperty(std::string(1, c));
    ASSERT_TRUE(sub.valid()) << c;
    ASSERT_EQ(base::Value::TYPE_BOOLEAN, sub.type()) << c;
  }
}

TEST(SchemaTest, AdditionalPropertiesReference) {
  // Verifies that "additionalProperties" can be a reference.
  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"policy\": {"
      "      \"type\": \"object\","
      "      \"properties\": {"
      "        \"foo\": {"
      "          \"type\": \"boolean\","
      "          \"id\": \"FooId\""
      "        }"
      "      },"
      "      \"additionalProperties\": { \"$ref\": \"FooId\" }"
      "    }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  Schema policy = schema.GetKnownProperty("policy");
  ASSERT_TRUE(policy.valid());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, policy.type());

  Schema foo = policy.GetKnownProperty("foo");
  ASSERT_TRUE(foo.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, foo.type());

  Schema additional = policy.GetAdditionalProperties();
  ASSERT_TRUE(additional.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, additional.type());

  Schema x = policy.GetProperty("x");
  ASSERT_TRUE(x.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, x.type());
}

TEST(SchemaTest, ItemsReference) {
  // Verifies that "items" can be a reference.
  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"foo\": {"
      "      \"type\": \"boolean\","
      "      \"id\": \"FooId\""
      "    },"
      "    \"list\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"$ref\": \"FooId\" }"
      "    }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema.valid()) << error;
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  Schema foo = schema.GetKnownProperty("foo");
  ASSERT_TRUE(foo.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, foo.type());

  Schema list = schema.GetKnownProperty("list");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::TYPE_LIST, list.type());

  Schema items = list.GetItems();
  ASSERT_TRUE(items.valid());
  ASSERT_EQ(base::Value::TYPE_BOOLEAN, items.type());
}


}  // namespace policy
