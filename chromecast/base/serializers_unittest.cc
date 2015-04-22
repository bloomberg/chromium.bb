// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {
const char kEmptyJsonString[] = "{}";
const char kProperJsonString[] =
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n";
const char kPoorlyFormedJsonString[] = "{\"key\":";
const char kTestKey[] = "test_key";
const char kTestValue[] = "test_value";

}  // namespace

TEST(DeserializeFromJson, EmptyString) {
  std::string str;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeFromJson, EmptyJsonObject) {
  std::string str = kEmptyJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, ProperJsonObject) {
  std::string str = kProperJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, PoorlyFormedJsonObject) {
  std::string str = kPoorlyFormedJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(SerializeToJson, BadValue) {
  base::BinaryValue value(scoped_ptr<char[]>(new char[12]), 12);
  scoped_ptr<std::string> str = SerializeToJson(value);
  EXPECT_EQ(nullptr, str.get());
}

TEST(SerializeToJson, EmptyValue) {
  base::DictionaryValue value;
  scoped_ptr<std::string> str = SerializeToJson(value);
  ASSERT_NE(nullptr, str.get());
  EXPECT_EQ(kEmptyJsonString, *str);
}

TEST(SerializeToJson, PopulatedValue) {
  base::DictionaryValue orig_value;
  orig_value.SetString(kTestKey, kTestValue);
  scoped_ptr<std::string> str = SerializeToJson(orig_value);
  ASSERT_NE(nullptr, str.get());

  scoped_ptr<base::Value> new_value = DeserializeFromJson(*str);
  ASSERT_NE(nullptr, new_value.get());
  EXPECT_TRUE(new_value->Equals(&orig_value));
}

}  // namespace chromecast
