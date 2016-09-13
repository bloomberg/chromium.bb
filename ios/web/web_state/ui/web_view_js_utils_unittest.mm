// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

// Tests that ValueResultFromWKResult converts nil value to nullptr.
TEST(WebViewJsUtilsTest, ValueResultFromUndefinedWKResult) {
  EXPECT_FALSE(ValueResultFromWKResult(nil));
}

// Tests that ValueResultFromWKResult converts string to Value::TYPE_STRING.
TEST(WebViewJsUtilsTest, ValueResultFromStringWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@"test"));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_STRING, value->GetType());
  std::string converted_result;
  value->GetAsString(&converted_result);
  EXPECT_EQ("test", converted_result);
}

// Tests that ValueResultFromWKResult converts inetger to Value::TYPE_DOUBLE.
// NOTE: WKWebView API returns all numbers as kCFNumberFloat64Type, so there is
// no way to tell if the result is integer or double.
TEST(WebViewJsUtilsTest, ValueResultFromIntegerWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@1));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_DOUBLE, value->GetType());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(1, converted_result);
}

// Tests that ValueResultFromWKResult converts double to Value::TYPE_DOUBLE.
TEST(WebViewJsUtilsTest, ValueResultFromDoubleWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@3.14));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_DOUBLE, value->GetType());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(3.14, converted_result);
}

// Tests that ValueResultFromWKResult converts bool to Value::TYPE_BOOLEAN.
TEST(WebViewJsUtilsTest, ValueResultFromBoolWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@YES));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, value->GetType());
  bool converted_result = false;
  value->GetAsBoolean(&converted_result);
  EXPECT_TRUE(converted_result);
}

// Tests that ValueResultFromWKResult converts null to Value::TYPE_NULL.
TEST(WebViewJsUtilsTest, ValueResultFromNullWKResult) {
  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult([NSNull null]));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_NULL, value->GetType());
}

// Tests that ValueResultFromWKResult converts NSDictionaries to properly
// initialized base::DictionaryValue.
TEST(WebViewJsUtilsTest, ValueResultFromDictionaryWKResult) {
  NSDictionary* test_dictionary =
      @{ @"Key1" : @"Value1",
         @"Key2" : @{@"Key3" : @42} };

  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult(test_dictionary));
  base::DictionaryValue* dictionary = nullptr;
  value->GetAsDictionary(&dictionary);
  EXPECT_NE(nullptr, dictionary);

  std::string value1;
  dictionary->GetString("Key1", &value1);
  EXPECT_EQ("Value1", value1);

  base::DictionaryValue const* inner_dictionary = nullptr;
  dictionary->GetDictionary("Key2", &inner_dictionary);
  EXPECT_NE(nullptr, inner_dictionary);

  double value3;
  inner_dictionary->GetDouble("Key3", &value3);
  EXPECT_EQ(42, value3);
}

// Tests that an NSDictionary with a cycle does not cause infinite recursion.
TEST(WebViewJsUtilsTest, ValueResultFromDictionaryWithDepthCheckWKResult) {
  // Create a dictionary with a cycle.
  NSMutableDictionary* test_dictionary =
      [NSMutableDictionary dictionaryWithCapacity:1];
  NSMutableDictionary* test_dictionary_2 =
      [NSMutableDictionary dictionaryWithCapacity:1];
  const char* key = "key";
  NSString* obj_c_key =
      [NSString stringWithCString:key encoding:NSASCIIStringEncoding];
  test_dictionary[obj_c_key] = test_dictionary_2;
  test_dictionary_2[obj_c_key] = test_dictionary;

  // Break the retain cycle so that the dictionaries are freed.
  base::ScopedClosureRunner runner(base::BindBlock(^{
    [test_dictionary_2 removeAllObjects];
  }));

  // Check that parsing the dictionary stopped at a depth of
  // |kMaximumParsingRecursionDepth|.
  std::unique_ptr<base::Value> value =
      web::ValueResultFromWKResult(test_dictionary);
  base::DictionaryValue* current_dictionary = nullptr;
  base::DictionaryValue* inner_dictionary = nullptr;

  value->GetAsDictionary(&current_dictionary);
  EXPECT_NE(nullptr, current_dictionary);

  for (int current_depth = 0; current_depth <= kMaximumParsingRecursionDepth;
       current_depth++) {
    EXPECT_NE(nullptr, current_dictionary);
    inner_dictionary = nullptr;
    current_dictionary->GetDictionary(key, &inner_dictionary);
    current_dictionary = inner_dictionary;
  }
  EXPECT_EQ(nullptr, current_dictionary);
}

}  // namespace web
