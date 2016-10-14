// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/renderer/argument_spec.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

// Returns a parsed version of |str|, substituting double quotes for single
// quotes.
std::unique_ptr<base::Value> GetValue(const std::string& str) {
  std::string updated;
  base::ReplaceChars(str.c_str(), "'", "\"", &updated);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(updated);
  CHECK(value);
  return value;
}

}  // namespace

class ArgumentSpecUnitTest : public gin::V8Test {
 protected:
  ArgumentSpecUnitTest() {}
  ~ArgumentSpecUnitTest() override {}
  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const std::string& expected_json_single_quotes) {
    std::string expected_json;
    base::ReplaceChars(expected_json_single_quotes.c_str(), "'", "\"",
                       &expected_json);
    RunTest(spec, script_source, TestResult::PASS, expected_json,
            std::string());
  }

  void ExpectFailure(const ArgumentSpec& spec,
                     const std::string& script_source) {
    RunTest(spec, script_source, TestResult::FAIL, std::string(),
            std::string());
  }

  void ExpectThrow(const ArgumentSpec& spec,
                   const std::string& script_source,
                   const std::string& expected_thrown_message) {
    RunTest(spec, script_source, TestResult::THROW, std::string(),
            expected_thrown_message);
  }

 private:
  enum class TestResult { PASS, FAIL, THROW, };

  void RunTest(const ArgumentSpec& spec,
               const std::string& script_source,
               TestResult expected_result,
               const std::string& expected_json,
               const std::string& expected_thrown_message);

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpecUnitTest);
};

void ArgumentSpecUnitTest::RunTest(const ArgumentSpec& spec,
                                   const std::string& script_source,
                                   TestResult expected_result,
                                   const std::string& expected_json,
                                   const std::string& expected_thrown_message) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(instance_->isolate());
  v8::Local<v8::String> source = gin::StringToV8(isolate, script_source);
  ASSERT_FALSE(source.IsEmpty());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(instance_->isolate(), context_);
  v8::Local<v8::Script> script = v8::Script::Compile(source);
  ASSERT_FALSE(script.IsEmpty()) << script_source;
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> val = script->Run();
  ASSERT_FALSE(val.IsEmpty()) << script_source;

  std::string error;
  std::unique_ptr<base::Value> out_value =
      spec.ConvertArgument(context, val, &error);
  bool should_succeed = expected_result == TestResult::PASS;
  ASSERT_EQ(should_succeed, !!out_value) << script_source << ", " << error;
  bool should_throw = expected_result == TestResult::THROW;
  ASSERT_EQ(should_throw, try_catch.HasCaught()) << script_source;
  if (should_succeed) {
    ASSERT_TRUE(out_value);
    std::string actual_json;
    EXPECT_TRUE(base::JSONWriter::Write(*out_value, &actual_json));
    EXPECT_EQ(expected_json, actual_json);
  } else if (should_throw) {
    EXPECT_EQ(expected_thrown_message,
              gin::V8ToString(try_catch.Message()->Get()));
  }
}

TEST_F(ArgumentSpecUnitTest, Test) {
  {
    ArgumentSpec spec(*GetValue("{'type': 'integer'}"));
    ExpectSuccess(spec, "1", "1");
    ExpectSuccess(spec, "-1", "-1");
    ExpectSuccess(spec, "0", "0");
    ExpectFailure(spec, "undefined");
    ExpectFailure(spec, "null");
    ExpectFailure(spec, "'foo'");
    ExpectFailure(spec, "'1'");
    ExpectFailure(spec, "{}");
    ExpectFailure(spec, "[1]");
  }

  {
    ArgumentSpec spec(*GetValue("{'type': 'integer', 'minimum': 1}"));
    ExpectSuccess(spec, "2", "2");
    ExpectSuccess(spec, "1", "1");
    ExpectFailure(spec, "0");
    ExpectFailure(spec, "-1");
  }

  {
    ArgumentSpec spec(*GetValue("{'type': 'string'}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "''", "''");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "{}");
    ExpectFailure(spec, "['foo']");
  }

  {
    ArgumentSpec spec(*GetValue("{'type': 'boolean'}"));
    ExpectSuccess(spec, "true", "true");
    ExpectSuccess(spec, "false", "false");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "'true'");
    ExpectFailure(spec, "null");
  }

  {
    ArgumentSpec spec(
        *GetValue("{'type': 'array', 'items': {'type': 'string'}}"));
    ExpectSuccess(spec, "[]", "[]");
    ExpectSuccess(spec, "['foo']", "['foo']");
    ExpectSuccess(spec, "['foo', 'bar']", "['foo','bar']");
    ExpectSuccess(spec, "var x = new Array(); x[0] = 'foo'; x;", "['foo']");
    ExpectFailure(spec, "'foo'");
    ExpectFailure(spec, "[1, 2]");
    ExpectFailure(spec, "['foo', 1]");
    ExpectThrow(
        spec,
        "var x = [];"
        "Object.defineProperty("
        "    x, 0,"
        "    { get: () => { throw new Error('Badness'); } });"
        "x;",
        "Uncaught Error: Badness");
  }

  {
    const char kObjectSpec[] =
        "{"
        "  'type': 'object',"
        "  'properties': {"
        "    'prop1': {'type': 'string'},"
        "    'prop2': {'type': 'integer', 'optional': true}"
        "  }"
        "}";
    ArgumentSpec spec(*GetValue(kObjectSpec));
    ExpectSuccess(spec, "({prop1: 'foo', prop2: 2})",
                  "{'prop1':'foo','prop2':2}");
    ExpectSuccess(spec, "({prop1: 'foo', prop2: 2, prop3: 'blah'})",
                  "{'prop1':'foo','prop2':2}");
    ExpectSuccess(spec, "({prop1: 'foo'})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "({prop1: 'foo', prop2: null})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "x = {}; x.prop1 = 'foo'; x;", "{'prop1':'foo'}");
    ExpectSuccess(
        spec,
        "function X() {}\n"
        "X.prototype = { prop1: 'foo' };\n"
        "function Y() { this.__proto__ = X.prototype; }\n"
        "var z = new Y();\n"
        "z;",
        "{'prop1':'foo'}");
    ExpectFailure(spec, "({prop1: 'foo', prop2: 'bar'})");
    ExpectFailure(spec, "({prop2: 2})");
    // Self-referential fun. Currently we don't have to worry about these much
    // because the spec won't match at some point (and V8ValueConverter has
    // cycle detection and will fail).
    ExpectFailure(spec, "x = {}; x.prop1 = x; x;");
    ExpectThrow(
        spec,
        "({ get prop1() { throw new Error('Badness'); }});",
        "Uncaught Error: Badness");
    ExpectThrow(
        spec,
        "x = {prop1: 'foo'};\n"
        "Object.defineProperty(\n"
        "    x, 'prop2',\n"
        "    { get: () => { throw new Error('Badness'); } });\n"
        "x;",
        "Uncaught Error: Badness");
  }
}

}  // namespace extensions
