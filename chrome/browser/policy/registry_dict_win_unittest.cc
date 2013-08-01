// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/registry_dict_win.h"

#include "base/values.h"
#include "chrome/common/json_schema/json_schema_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema = json_schema_constants;

namespace policy {
namespace {

TEST(RegistryDictTest, SetAndGetValue) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  test_dict.SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  EXPECT_EQ(1, test_dict.values().size());
  EXPECT_TRUE(base::Value::Equals(&int_value, test_dict.GetValue("one")));
  EXPECT_FALSE(test_dict.GetValue("two"));

  test_dict.SetValue("two", make_scoped_ptr(string_value.DeepCopy()));
  EXPECT_EQ(2, test_dict.values().size());
  EXPECT_TRUE(base::Value::Equals(&int_value, test_dict.GetValue("one")));
  EXPECT_TRUE(base::Value::Equals(&string_value, test_dict.GetValue("two")));

  scoped_ptr<base::Value> one(test_dict.RemoveValue("one"));
  EXPECT_EQ(1, test_dict.values().size());
  EXPECT_TRUE(base::Value::Equals(&int_value, one.get()));
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_TRUE(base::Value::Equals(&string_value, test_dict.GetValue("two")));

  test_dict.ClearValues();
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingValueNames) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  test_dict.SetValue("One", make_scoped_ptr(int_value.DeepCopy()));
  EXPECT_EQ(1, test_dict.values().size());
  EXPECT_TRUE(base::Value::Equals(&int_value, test_dict.GetValue("oNe")));

  RegistryDict::ValueMap::const_iterator entry = test_dict.values().begin();
  ASSERT_NE(entry, test_dict.values().end());
  EXPECT_EQ("One", entry->first);

  test_dict.SetValue("ONE", make_scoped_ptr(string_value.DeepCopy()));
  EXPECT_EQ(1, test_dict.values().size());
  EXPECT_TRUE(base::Value::Equals(&string_value, test_dict.GetValue("one")));

  scoped_ptr<base::Value> removed_value(test_dict.RemoveValue("onE"));
  EXPECT_TRUE(base::Value::Equals(&string_value, removed_value.get()));
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, SetAndGetKeys) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  scoped_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  test_dict.SetKey("two", subdict.Pass());
  EXPECT_EQ(1, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&int_value, actual_subdict->GetValue("one")));

  subdict.reset(new RegistryDict());
  subdict->SetValue("three", make_scoped_ptr(string_value.DeepCopy()));
  test_dict.SetKey("four", subdict.Pass());
  EXPECT_EQ(2, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&int_value, actual_subdict->GetValue("one")));
  actual_subdict = test_dict.GetKey("four");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&string_value,
                                  actual_subdict->GetValue("three")));

  test_dict.ClearKeys();
  EXPECT_FALSE(test_dict.GetKey("one"));
  EXPECT_FALSE(test_dict.GetKey("three"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingKeyNames) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);

  test_dict.SetKey("One", make_scoped_ptr(new RegistryDict()));
  EXPECT_EQ(1, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(actual_subdict->values().empty());

  RegistryDict::KeyMap::const_iterator entry = test_dict.keys().begin();
  ASSERT_NE(entry, test_dict.keys().end());
  EXPECT_EQ("One", entry->first);

  scoped_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", make_scoped_ptr(int_value.DeepCopy()));
  test_dict.SetKey("ONE", subdict.Pass());
  EXPECT_EQ(1, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&int_value,
                                  actual_subdict->GetValue("two")));

  scoped_ptr<RegistryDict> removed_key(test_dict.RemoveKey("one"));
  ASSERT_TRUE(removed_key);
  EXPECT_TRUE(base::Value::Equals(&int_value,
                                  removed_key->GetValue("two")));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, Merge) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  dict_a.SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  scoped_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", make_scoped_ptr(string_value.DeepCopy()));
  dict_a.SetKey("three", subdict.Pass());

  dict_b.SetValue("four", make_scoped_ptr(string_value.DeepCopy()));
  subdict.reset(new RegistryDict());
  subdict->SetValue("two", make_scoped_ptr(int_value.DeepCopy()));
  dict_b.SetKey("three", subdict.Pass());
  subdict.reset(new RegistryDict());
  subdict->SetValue("five", make_scoped_ptr(int_value.DeepCopy()));
  dict_b.SetKey("six", subdict.Pass());

  dict_a.Merge(dict_b);

  EXPECT_TRUE(base::Value::Equals(&int_value, dict_a.GetValue("one")));
  EXPECT_TRUE(base::Value::Equals(&string_value, dict_b.GetValue("four")));
  RegistryDict* actual_subdict = dict_a.GetKey("three");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&int_value, actual_subdict->GetValue("two")));
  actual_subdict = dict_a.GetKey("six");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&int_value,
                                  actual_subdict->GetValue("five")));
}

TEST(RegistryDictTest, Swap) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  dict_a.SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  dict_a.SetKey("two", make_scoped_ptr(new RegistryDict()));
  dict_b.SetValue("three", make_scoped_ptr(string_value.DeepCopy()));

  dict_a.Swap(&dict_b);

  EXPECT_TRUE(base::Value::Equals(&int_value, dict_b.GetValue("one")));
  EXPECT_TRUE(dict_b.GetKey("two"));
  EXPECT_FALSE(dict_b.GetValue("two"));

  EXPECT_TRUE(base::Value::Equals(&string_value, dict_a.GetValue("three")));
  EXPECT_FALSE(dict_a.GetValue("one"));
  EXPECT_FALSE(dict_a.GetKey("two"));
}

TEST(RegistryDictTest, ConvertToJSON) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  test_dict.SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  scoped_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", make_scoped_ptr(string_value.DeepCopy()));
  test_dict.SetKey("three", subdict.Pass());
  scoped_ptr<RegistryDict> list(new RegistryDict());
  list->SetValue("1", make_scoped_ptr(string_value.DeepCopy()));
  test_dict.SetKey("four", list.Pass());

  base::DictionaryValue schema;
  scoped_ptr<base::DictionaryValue> list_schema(new base::DictionaryValue());
  list_schema->SetString(schema::kType, schema::kArray);
  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue());
  properties->Set("four", list_schema.release());
  schema.SetString(schema::kType, schema::kObject);
  schema.Set(schema::kProperties, properties.release());

  scoped_ptr<base::Value> actual(test_dict.ConvertToJSON(&schema));

  base::DictionaryValue expected;
  expected.Set("one", int_value.DeepCopy());
  scoped_ptr<base::DictionaryValue> expected_subdict(
      new base::DictionaryValue());
  expected_subdict->Set("two", string_value.DeepCopy());
  expected.Set("three", expected_subdict.release());
  scoped_ptr<base::ListValue> expected_list(new base::ListValue());
  expected_list->Append(string_value.DeepCopy());
  expected.Set("four", expected_list.release());

  EXPECT_TRUE(base::Value::Equals(actual.get(), &expected));
}

TEST(RegistryDictTest, KeyValueNameClashes) {
  RegistryDict test_dict;

  base::FundamentalValue int_value(42);
  base::StringValue string_value("fortytwo");

  test_dict.SetValue("one", make_scoped_ptr(int_value.DeepCopy()));
  scoped_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", make_scoped_ptr(string_value.DeepCopy()));
  test_dict.SetKey("one", subdict.Pass());

  EXPECT_TRUE(base::Value::Equals(&int_value, test_dict.GetValue("one")));
  RegistryDict* actual_subdict = test_dict.GetKey("one");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(base::Value::Equals(&string_value,
                                  actual_subdict->GetValue("two")));
}

}  // namespace
}  // namespace policy
