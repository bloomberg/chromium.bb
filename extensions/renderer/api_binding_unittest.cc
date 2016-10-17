// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

// Function spec; we use single quotes for readability and then replace them.
const char kFunctions[] =
    "[{"
    "  'name': 'oneString',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }]"
    "}, {"
    "  'name': 'stringAndInt',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }, {"
    "     'type': 'integer',"
    "     'name': 'int'"
    "   }]"
    "}, {"
    "  'name': 'stringOptionalIntAndBool',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }, {"
    "     'type': 'integer',"
    "     'name': 'optionalint',"
    "     'optional': true"
    "   }, {"
    "     'type': 'boolean',"
    "     'name': 'bool'"
    "   }]"
    "}, {"
    "  'name': 'oneObject',"
    "  'parameters': [{"
    "    'type': 'object',"
    "    'name': 'foo',"
    "    'properties': {"
    "      'prop1': {'type': 'string'},"
    "      'prop2': {'type': 'string', 'optional': true}"
    "    }"
    "  }]"
    "}]";

const char kError[] = "Uncaught TypeError: Invalid invocation";

}  // namespace

class APIBindingTest : public gin::V8Test {
 public:
  void OnFunctionCall(const std::string& name,
                      std::unique_ptr<base::ListValue> arguments) {
    arguments_ = std::move(arguments);
  }

 protected:
  APIBindingTest() {}
  void SetUp() override {
    gin::V8Test::SetUp();
    v8::HandleScope handle_scope(instance_->isolate());
    holder_ = base::MakeUnique<gin::ContextHolder>(instance_->isolate());
    holder_->SetContext(
        v8::Local<v8::Context>::New(instance_->isolate(), context_));
  }

  void TearDown() override {
    holder_.reset();
    gin::V8Test::TearDown();
  }

  void ExpectPass(v8::Local<v8::Object> object,
                  const std::string& script_source,
                  const std::string& expected_json_arguments_single_quotes) {
    RunTest(object, script_source, true,
            ReplaceSingleQuotes(expected_json_arguments_single_quotes),
            std::string());
  }

  void ExpectFailure(v8::Local<v8::Object> object,
                     const std::string& script_source,
                     const std::string& expected_error) {
    RunTest(object, script_source, false, std::string(), expected_error);
  }

 private:
  void RunTest(v8::Local<v8::Object> object,
               const std::string& script_source,
               bool should_pass,
               const std::string& expected_json_arguments,
               const std::string& expected_error);

  std::unique_ptr<base::ListValue> arguments_;
  std::unique_ptr<gin::ContextHolder> holder_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingTest);
};

void APIBindingTest::RunTest(v8::Local<v8::Object> object,
                             const std::string& script_source,
                             bool should_pass,
                             const std::string& expected_json_arguments,
                             const std::string& expected_error) {
  EXPECT_FALSE(arguments_);
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());
  v8::Isolate* isolate = instance_->isolate();

  v8::Local<v8::Function> func =
      FunctionFromString(isolate, wrapped_script_source);
  ASSERT_FALSE(func.IsEmpty());

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> argv[] = {object};
  func->Call(v8::Undefined(isolate), 1, argv);

  if (should_pass) {
    EXPECT_FALSE(try_catch.HasCaught())
        << gin::V8ToString(try_catch.Message()->Get());
    ASSERT_TRUE(arguments_) << script_source;
    EXPECT_EQ(expected_json_arguments, ValueToString(*arguments_));
  } else {
    ASSERT_TRUE(try_catch.HasCaught()) << script_source;
    std::string message = gin::V8ToString(try_catch.Message()->Get());
    EXPECT_EQ(expected_error, message);
  }

  arguments_.reset();
}

TEST_F(APIBindingTest, Test) {
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, base::ListValue(),
      base::Bind(&APIBindingTest::OnFunctionCall, base::Unretained(this)),
      &refs);
  EXPECT_TRUE(refs.empty());

  v8::Isolate* isolate = instance_->isolate();

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);

  v8::Local<v8::Object> binding_object =
      binding.CreateInstance(context, isolate);

  ExpectPass(binding_object, "obj.oneString('foo');", "['foo']");
  ExpectPass(binding_object, "obj.oneString('');", "['']");
  ExpectFailure(binding_object, "obj.oneString(1);", kError);
  ExpectFailure(binding_object, "obj.oneString();", kError);
  ExpectFailure(binding_object, "obj.oneString({});", kError);
  ExpectFailure(binding_object, "obj.oneString('foo', 'bar');", kError);

  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
  ExpectPass(binding_object, "obj.stringAndInt('foo', -1);", "['foo',-1]");
  ExpectFailure(binding_object, "obj.stringAndInt(1);", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt(1, 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', '1');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 2.3);", kError);

  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', 42, true);",
             "['foo',42,true]");
  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', true);",
             "['foo',null,true]");
  ExpectFailure(binding_object,
                "obj.stringOptionalIntAndBool('foo', 'bar', true);", kError);

  ExpectPass(binding_object,
             "obj.oneObject({prop1: 'foo'});", "[{'prop1':'foo'}]");
  ExpectFailure(
      binding_object,
      "obj.oneObject({ get prop1() { throw new Error('Badness'); } });",
      "Uncaught Error: Badness");
}

TEST_F(APIBindingTest, TypeRefsTest) {
  const char kTypes[] =
      "[{"
      "  'id': 'refObj',"
      "  'type': 'object',"
      "  'properties': {"
      "    'prop1': {'type': 'string'},"
      "    'prop2': {'type': 'integer', 'optional': true}"
      "  }"
      "}, {"
      "  'id': 'refEnum',"
      "  'type': 'string',"
      "  'enum': ['alpha', 'beta']"
      "}]";
  const char kRefFunctions[] =
      "[{"
      "  'name': 'takesRefObj',"
      "  'parameters': [{"
      "    'name': 'o',"
      "    '$ref': 'refObj'"
      "  }]"
      "}, {"
      "  'name': 'takesRefEnum',"
      "  'parameters': [{"
      "    'name': 'e',"
      "    '$ref': 'refEnum'"
      "   }]"
      "}]";

  std::unique_ptr<base::ListValue> functions =
      ListValueFromString(kRefFunctions);
  ASSERT_TRUE(functions);
  std::unique_ptr<base::ListValue> types = ListValueFromString(kTypes);
  ASSERT_TRUE(types);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, *types,
      base::Bind(&APIBindingTest::OnFunctionCall, base::Unretained(this)),
      &refs);
  EXPECT_EQ(2u, refs.size());
  EXPECT_TRUE(base::ContainsKey(refs, "refObj"));
  EXPECT_TRUE(base::ContainsKey(refs, "refEnum"));

  v8::Isolate* isolate = instance_->isolate();

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);

  v8::Local<v8::Object> binding_object =
      binding.CreateInstance(context, isolate);

  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo'})",
             "[{'prop1':'foo'}]");
  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 2})",
             "[{'prop1':'foo','prop2':2}]");
  ExpectFailure(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 'a'})",
                kError);
  ExpectPass(binding_object, "obj.takesRefEnum('alpha')", "['alpha']");
  ExpectPass(binding_object, "obj.takesRefEnum('beta')", "['beta']");
  ExpectFailure(binding_object, "obj.takesRefEnum('gamma')", kError);
}

}  // namespace extensions
