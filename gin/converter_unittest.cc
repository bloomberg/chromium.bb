// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/converter.h"

#include <limits.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

using v8::Array;
using v8::Boolean;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace gin {

typedef V8Test ConverterTest;

TEST_F(ConverterTest, Bool) {
  HandleScope handle_scope(isolate_);

  EXPECT_TRUE(Converter<bool>::ToV8(isolate_, true)->StrictEquals(
      Boolean::New(true)));
  EXPECT_TRUE(Converter<bool>::ToV8(isolate_, false)->StrictEquals(
      Boolean::New(false)));

  struct {
    Handle<Value> input;
    bool expected;
  } test_data[] = {
    { Boolean::New(false).As<Value>(), false },
    { Boolean::New(true).As<Value>(), true },
    { Number::New(0).As<Value>(), false },
    { Number::New(1).As<Value>(), true },
    { Number::New(-1).As<Value>(), true },
    { Number::New(0.1).As<Value>(), true },
    { String::New("").As<Value>(), false },
    { String::New("foo").As<Value>(), true },
    { Object::New().As<Value>(), true },
    { Null().As<Value>(), false },
    { Undefined().As<Value>(), false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    bool result = false;
    EXPECT_TRUE(Converter<bool>::FromV8(test_data[i].input, &result));
    EXPECT_EQ(test_data[i].expected, result);

    result = true;
    EXPECT_TRUE(Converter<bool>::FromV8(test_data[i].input, &result));
    EXPECT_EQ(test_data[i].expected, result);
  }
}

TEST_F(ConverterTest, Int32) {
  HandleScope handle_scope(isolate_);

  int test_data_to[] = {-1, 0, 1};
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data_to); ++i) {
    EXPECT_TRUE(Converter<int32_t>::ToV8(isolate_,
        test_data_to[i])->StrictEquals(Integer::New(test_data_to[i])));
  }

  struct {
    v8::Handle<v8::Value> input;
    bool expect_sucess;
    int expected_result;
  } test_data_from[] = {
    { Boolean::New(false).As<Value>(), false, 0 },
    { Boolean::New(true).As<Value>(), false, 0 },
    { Integer::New(-1).As<Value>(), true, -1 },
    { Integer::New(0).As<Value>(), true, 0 },
    { Integer::New(1).As<Value>(), true, 1 },
    { Number::New(-1).As<Value>(), true, -1 },
    { Number::New(1.1).As<Value>(), false, 0 },
    { String::New("42").As<Value>(), false, 0 },
    { String::New("foo").As<Value>(), false, 0 },
    { Object::New().As<Value>(), false, 0 },
    { Array::New().As<Value>(), false, 0 },
    { v8::Null().As<Value>(), false, 0 },
    { v8::Undefined().As<Value>(), false, 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data_from); ++i) {
    int32_t result = std::numeric_limits<int32_t>::min();
    bool success = Converter<int32_t>::FromV8(test_data_from[i].input, &result);
    EXPECT_EQ(test_data_from[i].expect_sucess, success) << i;
    if (success)
      EXPECT_EQ(test_data_from[i].expected_result, result) << i;
  }
}

TEST_F(ConverterTest, Vector) {
  HandleScope handle_scope(isolate_);

  std::vector<int> expected;
  expected.push_back(-1);
  expected.push_back(0);
  expected.push_back(1);

  Handle<Array> js_array = Handle<Array>::Cast(
      Converter<std::vector<int> >::ToV8(isolate_, expected));
  ASSERT_FALSE(js_array.IsEmpty());
  EXPECT_EQ(3u, js_array->Length());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_TRUE(Integer::New(expected[i])->StrictEquals(
        js_array->Get(static_cast<int>(i))));
  }

  std::vector<int> actual;
  EXPECT_TRUE(Converter<std::vector<int> >::FromV8(js_array, &actual));
  EXPECT_EQ(expected, actual);
}

}  // namespace gin
