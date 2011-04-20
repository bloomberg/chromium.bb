// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "content/renderer/v8_value_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

class V8ValueConverterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    v8::HandleScope handle_scope;
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    context_ = v8::Context::New(NULL, global);
  }

  virtual void TearDown() {
    context_.Dispose();
  }

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

TEST_F(V8ValueConverterTest, BasicRoundTrip) {
  DictionaryValue original_root;
  original_root.Set("null", Value::CreateNullValue());
  original_root.Set("true", Value::CreateBooleanValue(true));
  original_root.Set("false", Value::CreateBooleanValue(false));
  original_root.Set("positive-int", Value::CreateIntegerValue(42));
  original_root.Set("negative-int", Value::CreateIntegerValue(-42));
  original_root.Set("zero", Value::CreateIntegerValue(0));
  original_root.Set("double", Value::CreateDoubleValue(88.8));
  original_root.Set("big-integral-double",
                    Value::CreateDoubleValue(pow(2.0, 53)));
  original_root.Set("string", Value::CreateStringValue("foobar"));
  original_root.Set("empty-string", Value::CreateStringValue(""));

  DictionaryValue* original_sub1 = new DictionaryValue();
  original_sub1->Set("foo", Value::CreateStringValue("bar"));
  original_sub1->Set("hot", Value::CreateStringValue("dog"));
  original_root.Set("dictionary", original_sub1);
  original_root.Set("empty-dictionary", new DictionaryValue());

  ListValue* original_sub2 = new ListValue();
  original_sub2->Append(Value::CreateStringValue("monkey"));
  original_sub2->Append(Value::CreateStringValue("balls"));
  original_root.Set("list", original_sub2);
  original_root.Set("empty-list", new ListValue());

  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  V8ValueConverter converter;
  v8::Handle<v8::Object> v8_object(
      v8::Handle<v8::Object>::Cast(
          converter.ToV8Value(&original_root, context_)));
  ASSERT_FALSE(v8_object.IsEmpty());

  EXPECT_EQ(original_root.size(), v8_object->GetPropertyNames()->Length());
  EXPECT_TRUE(v8_object->Get(v8::String::New("null"))->IsNull());
  EXPECT_TRUE(v8_object->Get(v8::String::New("true"))->IsTrue());
  EXPECT_TRUE(v8_object->Get(v8::String::New("false"))->IsFalse());
  EXPECT_TRUE(v8_object->Get(v8::String::New("positive-int"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("negative-int"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("zero"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("double"))->IsNumber());
  EXPECT_TRUE(
      v8_object->Get(v8::String::New("big-integral-double"))->IsNumber());
  EXPECT_TRUE(v8_object->Get(v8::String::New("string"))->IsString());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-string"))->IsString());
  EXPECT_TRUE(v8_object->Get(v8::String::New("dictionary"))->IsObject());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-dictionary"))->IsObject());
  EXPECT_TRUE(v8_object->Get(v8::String::New("list"))->IsArray());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-list"))->IsArray());

  scoped_ptr<Value> new_root(converter.FromV8Value(v8_object, context_));
  EXPECT_NE(&original_root, new_root.get());
  EXPECT_TRUE(original_root.Equals(new_root.get()));
}

TEST_F(V8ValueConverterTest, KeysWithDots) {
  DictionaryValue original;
  original.SetWithoutPathExpansion("foo.bar", Value::CreateStringValue("baz"));

  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  V8ValueConverter converter;
  scoped_ptr<Value> copy(
      converter.FromV8Value(
          converter.ToV8Value(&original, context_), context_));

  EXPECT_TRUE(original.Equals(copy.get()));
}
