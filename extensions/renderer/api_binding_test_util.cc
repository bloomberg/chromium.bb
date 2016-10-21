// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_test_util.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "gin/converter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

std::string ReplaceSingleQuotes(base::StringPiece str) {
  std::string result;
  base::ReplaceChars(str.as_string(), "'", "\"", &result);
  return result;
}

std::unique_ptr<base::Value> ValueFromString(base::StringPiece str) {
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(ReplaceSingleQuotes(str));
  EXPECT_TRUE(value) << str;
  return value;
}

std::unique_ptr<base::ListValue> ListValueFromString(base::StringPiece str) {
  return base::ListValue::From(ValueFromString(str));
}

std::unique_ptr<base::DictionaryValue> DictionaryValueFromString(
    base::StringPiece str) {
  return base::DictionaryValue::From(ValueFromString(str));
}

std::string ValueToString(const base::Value& value) {
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(value, &json));
  return json;
}

v8::Local<v8::Value> V8ValueFromScriptSource(v8::Local<v8::Context> context,
                                             base::StringPiece source) {
  v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(
      context, gin::StringToV8(context->GetIsolate(), source));
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script))
    return v8::Local<v8::Value>();
  return script->Run();
}

v8::Local<v8::Function> FunctionFromString(v8::Local<v8::Context> context,
                                           base::StringPiece source) {
  v8::Local<v8::Value> value = V8ValueFromScriptSource(context, source);
  v8::Local<v8::Function> function;
  EXPECT_TRUE(gin::ConvertFromV8(context->GetIsolate(), value, &function));
  return function;
}

std::unique_ptr<base::Value> V8ToBaseValue(v8::Local<v8::Value> value,
                                           v8::Local<v8::Context> context) {
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  return converter->FromV8Value(value, context);
}

}  // namespace extensions
