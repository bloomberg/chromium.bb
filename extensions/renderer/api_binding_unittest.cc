// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_request_handler.h"
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
    "}, {"
    "  'name': 'noArgs',"
    "  'parameters': []"
    "}, {"
    "  'name': 'intAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer'"
    "  }, {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }]"
    "}, {"
    "  'name': 'optionalIntAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer',"
    "    'optional': true"
    "  }, {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }]"
    "}, {"
    "  'name': 'optionalCallback',"
    "  'parameters': [{"
    "    'name': 'callback',"
    "    'type': 'function',"
    "    'optional': true"
    "  }]"
    "}]";

const char kError[] = "Uncaught TypeError: Invalid invocation";

}  // namespace

class APIBindingTest : public gin::V8Test {
 public:
  void OnFunctionCall(const std::string& name,
                      std::unique_ptr<base::ListValue> arguments,
                      v8::Isolate* isolate,
                      v8::Local<v8::Context> context,
                      v8::Local<v8::Function> callback) {
    last_request_id_.clear();
    arguments_ = std::move(arguments);
    if (!callback.IsEmpty()) {
      last_request_id_ =
          request_handler_->AddPendingRequest(isolate, callback, context);
    }
  }

 protected:
  APIBindingTest() {}
  void SetUp() override {
    gin::V8Test::SetUp();
    v8::HandleScope handle_scope(instance_->isolate());
    holder_ = base::MakeUnique<gin::ContextHolder>(instance_->isolate());
    holder_->SetContext(
        v8::Local<v8::Context>::New(instance_->isolate(), context_));
    request_handler_ = base::MakeUnique<APIRequestHandler>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  }

  void TearDown() override {
    request_handler_.reset();
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

  const std::string& last_request_id() const { return last_request_id_; }
  APIRequestHandler* request_handler() { return request_handler_.get(); }

 private:
  void RunTest(v8::Local<v8::Object> object,
               const std::string& script_source,
               bool should_pass,
               const std::string& expected_json_arguments,
               const std::string& expected_error);

  std::unique_ptr<base::ListValue> arguments_;
  std::unique_ptr<gin::ContextHolder> holder_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  std::string last_request_id_;

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

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  ASSERT_FALSE(func.IsEmpty());

  v8::Local<v8::Value> argv[] = {object};

  if (should_pass) {
    RunFunction(func, context, 1, argv);
    ASSERT_TRUE(arguments_) << script_source;
    EXPECT_EQ(expected_json_arguments, ValueToString(*arguments_));
  } else {
    RunFunctionAndExpectError(func, context, 1, argv, expected_error);
    EXPECT_FALSE(arguments_);
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

  ExpectPass(binding_object, "obj.noArgs()", "[]");
  ExpectFailure(binding_object, "obj.noArgs(0)", kError);
  ExpectFailure(binding_object, "obj.noArgs('')", kError);
  ExpectFailure(binding_object, "obj.noArgs(null)", kError);
  ExpectFailure(binding_object, "obj.noArgs(undefined)", kError);

  ExpectPass(binding_object, "obj.intAndCallback(1, function() {})", "[1]");
  ExpectFailure(binding_object, "obj.intAndCallback(function() {})", kError);
  ExpectFailure(binding_object, "obj.intAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalIntAndCallback(1, function() {})",
             "[1]");
  ExpectPass(binding_object, "obj.optionalIntAndCallback(function() {})",
             "[null]");
  ExpectFailure(binding_object, "obj.optionalIntAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalCallback(function() {})", "[]");
  ExpectPass(binding_object, "obj.optionalCallback()", "[]");
  ExpectPass(binding_object, "obj.optionalCallback(undefined)", "[]");
  ExpectFailure(binding_object, "obj.optionalCallback(0)", kError);
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

// TODO(devlin): Once we have an object that encompasses all these pieces (the
// APIBinding, ArgumentSpec::RefMap, and APIRequestHandler), we should move this
// test.
TEST_F(APIBindingTest, Callbacks) {
  const char kTestCall[] =
      "obj.functionWithCallback('foo', function() {\n"
      "  this.callbackArguments = Array.from(arguments);\n"
      "});";

  const char kFunctionSpec[] =
      "[{"
      "  'name': 'functionWithCallback',"
      "  'parameters': [{"
      "    'name': 'str',"
      "    'type': 'string'"
      "  }, {"
      "    'name': 'callback',"
      "    'type': 'function'"
      "  }]"
      "}]";

  std::unique_ptr<base::ListValue> functions =
      ListValueFromString(kFunctionSpec);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, base::ListValue(),
      base::Bind(&APIBindingTest::OnFunctionCall, base::Unretained(this)),
      &refs);

  v8::Isolate* isolate = instance_->isolate();

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);

  v8::Local<v8::Object> binding_object =
      binding.CreateInstance(context, isolate);

  ExpectPass(binding_object, kTestCall, "['foo']");
  ASSERT_FALSE(last_request_id().empty());
  const char kResponseArgsJson[] = "['response',1,{'key':42}]";
  std::unique_ptr<base::ListValue> expected_args =
      ListValueFromString(kResponseArgsJson);
  request_handler()->CompleteRequest(last_request_id(), *expected_args);

  v8::Local<v8::Value> res =
      GetPropertyFromObject(context->Global(), context, "callbackArguments");

  std::unique_ptr<base::Value> out_val = V8ToBaseValue(res, context);
  ASSERT_TRUE(out_val);
  EXPECT_EQ(ReplaceSingleQuotes(kResponseArgsJson), ValueToString(*out_val));
}

}  // namespace extensions
