// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/renderer/extensions/activity_log_converter_strategy.h"
#include "chrome/renderer/extensions/scoped_persistent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

class ActivityLogConverterStrategyTest : public testing::Test {
 public:
  ActivityLogConverterStrategyTest()
      : isolate_(v8::Isolate::GetCurrent())
      , handle_scope_(isolate_)
      , context_(v8::Context::New(isolate_))
      , context_scope_(context_.get()) {
  }

 protected:
  virtual void SetUp() {
    converter_.reset(V8ValueConverter::create());
    strategy_.reset(new ActivityLogConverterStrategy());
    converter_->SetFunctionAllowed(true);
    converter_->SetStrategy(strategy_.get());
  }

  testing::AssertionResult VerifyNull(v8::Local<v8::Value> v8_value) {
    scoped_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context_.get()));
    if (value->IsType(base::Value::TYPE_NULL))
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyBoolean(v8::Local<v8::Value> v8_value,
                                         bool expected) {
    bool out;
    scoped_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context_.get()));
    if (value->IsType(base::Value::TYPE_BOOLEAN)
        && value->GetAsBoolean(&out)
        && out == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyInteger(v8::Local<v8::Value> v8_value,
                                         int expected) {
    int out;
    scoped_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context_.get()));
    if (value->IsType(base::Value::TYPE_INTEGER)
        && value->GetAsInteger(&out)
        && out == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyDouble(v8::Local<v8::Value> v8_value,
                                        double expected) {
    double out;
    scoped_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context_.get()));
    if (value->IsType(base::Value::TYPE_DOUBLE)
        && value->GetAsDouble(&out)
        && out == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  testing::AssertionResult VerifyString(v8::Local<v8::Value> v8_value,
                                        const std::string& expected) {
    std::string out;
    scoped_ptr<base::Value> value(
        converter_->FromV8Value(v8_value, context_.get()));
    if (value->IsType(base::Value::TYPE_STRING)
        && value->GetAsString(&out)
        && out == expected)
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

  v8::Isolate* isolate_;
  v8::HandleScope handle_scope_;
  ScopedPersistent<v8::Context> context_;
  v8::Context::Scope context_scope_;
  scoped_ptr<V8ValueConverter> converter_;
  scoped_ptr<ActivityLogConverterStrategy> strategy_;
};

TEST_F(ActivityLogConverterStrategyTest, ConversionTest) {
  const char* source = "(function() {"
      "function foo() {}"
      "return {"
        "null: null,"
        "true: true,"
        "false: false,"
        "positive_int: 42,"
        "negative_int: -42,"
        "zero: 0,"
        "double: 88.8,"
        "big_integral_double: 9007199254740992.0,"  // 2.0^53
        "string: \"foobar\","
        "empty_string: \"\","
        "dictionary: {"
          "foo: \"bar\","
          "hot: \"dog\","
        "},"
        "empty_dictionary: {},"
        "list: [ \"monkey\", \"balls\" ],"
        "empty_list: [],"
        "function: function() {},"
        "named_function: foo"
      "};"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Object> v8_object = script->Run().As<v8::Object>();

  EXPECT_TRUE(VerifyString(v8_object, "[Object]"));
  EXPECT_TRUE(VerifyNull(v8_object->Get(v8::String::New("null"))));
  EXPECT_TRUE(VerifyBoolean(v8_object->Get(v8::String::New("true")), true));
  EXPECT_TRUE(VerifyBoolean(v8_object->Get(v8::String::New("false")), false));
  EXPECT_TRUE(VerifyInteger(v8_object->Get(v8::String::New("positive_int")),
                            42));
  EXPECT_TRUE(VerifyInteger(v8_object->Get(v8::String::New("negative_int")),
                            -42));
  EXPECT_TRUE(VerifyInteger(v8_object->Get(v8::String::New("zero")), 0));
  EXPECT_TRUE(VerifyDouble(v8_object->Get(v8::String::New("double")), 88.8));
  EXPECT_TRUE(VerifyDouble(
      v8_object->Get(v8::String::New("big_integral_double")),
      9007199254740992.0));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("string")),
                           "foobar"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("empty_string")),
                           ""));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("dictionary")),
                           "[Object]"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("empty_dictionary")),
                           "[Object]"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("list")),
                           "[Array]"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("empty_list")),
                           "[Array]"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("function")),
                           "[Function]"));
  EXPECT_TRUE(VerifyString(v8_object->Get(v8::String::New("named_function")),
                           "[Function foo()]"));
}

}  // namespace extensions

