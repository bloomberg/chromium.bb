// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_hooks.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_request_handler.h"
#include "gin/arguments.h"
#include "gin/converter.h"
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

bool AllowAllAPIs(const std::string& name) {
  return true;
}

}  // namespace

class APIBindingUnittest : public APIBindingTest {
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
  APIBindingUnittest() {}
  void SetUp() override {
    APIBindingTest::SetUp();
    request_handler_ = base::MakeUnique<APIRequestHandler>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  }

  void TearDown() override {
    request_handler_.reset();
    APIBindingTest::TearDown();
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

  bool HandlerWasInvoked() const { return arguments_ != nullptr; }
  const std::string& last_request_id() const { return last_request_id_; }
  APIRequestHandler* request_handler() { return request_handler_.get(); }

 private:
  void RunTest(v8::Local<v8::Object> object,
               const std::string& script_source,
               bool should_pass,
               const std::string& expected_json_arguments,
               const std::string& expected_error);

  std::unique_ptr<base::ListValue> arguments_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  std::string last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingUnittest);
};

void APIBindingUnittest::RunTest(v8::Local<v8::Object> object,
                                 const std::string& script_source,
                                 bool should_pass,
                                 const std::string& expected_json_arguments,
                                 const std::string& expected_error) {
  EXPECT_FALSE(arguments_);
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Context> context = ContextLocal();
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

TEST_F(APIBindingUnittest, TestEmptyAPI) {
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "empty", nullptr, nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);
  EXPECT_TRUE(refs.empty());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));
  EXPECT_EQ(
      0u,
      binding_object->GetOwnPropertyNames(context).ToLocalChecked()->Length());
}

TEST_F(APIBindingUnittest, Test) {
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);
  EXPECT_TRUE(refs.empty());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

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

TEST_F(APIBindingUnittest, TypeRefsTest) {
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
      "test", functions.get(), types.get(), nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);
  EXPECT_EQ(2u, refs.size());
  EXPECT_TRUE(base::ContainsKey(refs, "refObj"));
  EXPECT_TRUE(base::ContainsKey(refs, "refEnum"));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

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

TEST_F(APIBindingUnittest, RestrictedAPIs) {
  const char kRestrictedFunctions[] =
      "[{"
      "  'name': 'allowedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'allowedTwo',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedTwo',"
      "  'parameters': []"
      "}]";
  std::unique_ptr<base::ListValue> functions =
      ListValueFromString(kRestrictedFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  auto is_available = [](const std::string& name) {
    std::set<std::string> functions = {"test.allowedOne", "test.allowedTwo",
                                       "test.restrictedOne",
                                       "test.restrictedTwo"};
    EXPECT_TRUE(functions.count(name));
    return name == "test.allowedOne" || name == "test.allowedTwo";
  };

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(is_available));

  auto is_defined = [&binding_object, context](const std::string& name) {
    v8::Local<v8::Value> val =
        GetPropertyFromObject(binding_object, context, name);
    EXPECT_FALSE(val.IsEmpty());
    return !val->IsUndefined() && !val->IsNull();
  };

  EXPECT_TRUE(is_defined("allowedOne"));
  EXPECT_TRUE(is_defined("allowedTwo"));
  EXPECT_FALSE(is_defined("restrictedOne"));
  EXPECT_FALSE(is_defined("restrictedTwo"));
}

// Tests that events specified in the API are created as properties of the API
// object.
TEST_F(APIBindingUnittest, TestEventCreation) {
  const char kEvents[] = "[{'name': 'onFoo'}, {'name': 'onBar'}]";
  std::unique_ptr<base::ListValue> events = ListValueFromString(kEvents);
  ASSERT_TRUE(events);
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", functions.get(), nullptr, events.get(),
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  // Event behavior is tested in the APIEventHandler unittests as well as the
  // APIBindingsSystem tests, so we really only need to check that the events
  // are being initialized on the object.
  v8::Maybe<bool> has_on_foo =
      binding_object->Has(context, gin::StringToV8(isolate(), "onFoo"));
  EXPECT_TRUE(has_on_foo.IsJust());
  EXPECT_TRUE(has_on_foo.FromJust());

  v8::Maybe<bool> has_on_bar =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBar"));
  EXPECT_TRUE(has_on_bar.IsJust());
  EXPECT_TRUE(has_on_bar.FromJust());

  v8::Maybe<bool> has_on_baz =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBaz"));
  EXPECT_TRUE(has_on_baz.IsJust());
  EXPECT_FALSE(has_on_baz.FromJust());
}

TEST_F(APIBindingUnittest, TestDisposedContext) {
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      base::MakeUnique<APIBindingHooks>(binding::RunJSFunctionSync()), &refs);
  EXPECT_TRUE(refs.empty());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> argv[] = {binding_object};
  DisposeContext();
  RunFunction(func, context, arraysize(argv), argv);
  EXPECT_FALSE(HandlerWasInvoked());
  // This test passes if this does not crash, even under AddressSanitizer
  // builds.
}

// Tests adding custom hooks for an API method.
TEST_F(APIBindingUnittest, TestCustomHooks) {
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      base::Bind(&RunFunctionOnGlobalAndReturnHandle));
  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 std::vector<v8::Local<v8::Value>>* arguments,
                 const ArgumentSpec::RefMap& ref_map) {
    *did_call = true;
    if (arguments->size() != 1u) {
      EXPECT_EQ(1u, arguments->size());
      return APIBindingHooks::RequestResult::HANDLED;
    }
    EXPECT_EQ("foo", gin::V8ToString(arguments->at(0)));
    return APIBindingHooks::RequestResult::HANDLED;
  };
  hooks->RegisterHandleRequest("test.oneString", base::Bind(hook, &did_call));

  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      std::move(hooks), &refs);
  EXPECT_TRUE(refs.empty());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  // First try calling the oneString() method, which has a custom hook
  // installed.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);
  EXPECT_TRUE(did_call);

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
}

TEST_F(APIBindingUnittest, TestJSCustomHook) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      base::Bind(&RunFunctionOnGlobalAndReturnHandle));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setHandleRequest('oneString', function() {\n"
      "    this.requestArguments = Array.from(arguments);\n"
      "  });\n"
      "})";
  v8::Local<v8::String> source_string =
      gin::StringToV8(isolate(), kRegisterHook);
  v8::Local<v8::String> source_name =
      gin::StringToV8(isolate(), "custom_hook");
  hooks->RegisterJsSource(
      v8::Global<v8::String>(isolate(), source_string),
      v8::Global<v8::String>(isolate(), source_name));

  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;

  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      std::move(hooks), &refs);
  EXPECT_TRUE(refs.empty());

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  // First try calling with an invalid invocation. An error should be raised and
  // the hook should never have been called, since the arguments didn't match.
  ExpectFailure(binding_object, "obj.oneString(1);", kError);
  v8::Local<v8::Value> property =
      GetPropertyFromObject(context->Global(), context, "requestArguments");
  ASSERT_FALSE(property.IsEmpty());
  EXPECT_TRUE(property->IsUndefined());

  // Try calling the oneString() method with valid arguments. The hook should
  // be called.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);

  std::unique_ptr<base::Value> response_args =
      GetBaseValuePropertyFromObject(context->Global(), context,
                                     "requestArguments");
  ASSERT_TRUE(response_args);
  EXPECT_EQ("[\"foo\"]", ValueToString(*response_args));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
}

// Tests the updateArgumentsPreValidate hook.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPreValidate) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      base::Bind(&RunFunctionOnGlobalAndReturnHandle));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setUpdateArgumentsPreValidate('oneString', function() {\n"
      "    this.requestArguments = Array.from(arguments);\n"
      "    if (this.requestArguments[0] === true)\n"
      "      return ['hooked']\n"
      "    return this.requestArguments\n"
      "  });\n"
      "})";
  v8::Local<v8::String> source_string =
      gin::StringToV8(isolate(), kRegisterHook);
  v8::Local<v8::String> source_name =
      gin::StringToV8(isolate(), "custom_hook");
  hooks->RegisterJsSource(
      v8::Global<v8::String>(isolate(), source_string),
      v8::Global<v8::String>(isolate(), source_name));

  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;

  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      std::move(hooks), &refs);
  EXPECT_TRUE(refs.empty());

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  // Call the method with a hook. Since the hook updates arguments before
  // validation, we should be able to pass in invalid arguments and still
  // have the hook called.
  ExpectFailure(binding_object, "obj.oneString(false);", kError);
  EXPECT_EQ("[false]", GetStringPropertyFromObject(
                           context->Global(), context, "requestArguments"));

  ExpectPass(binding_object, "obj.oneString(true);", "['hooked']");
  EXPECT_EQ("[true]", GetStringPropertyFromObject(
                          context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
}

// Tests the updateArgumentsPreValidate hook.
TEST_F(APIBindingUnittest, TestThrowInUpdateArgumentsPreValidate) {
  auto run_js_and_allow_error = [](v8::Local<v8::Function> function,
                                   v8::Local<v8::Context> context,
                                   int argc,
                                   v8::Local<v8::Value> argv[]) {
    v8::MaybeLocal<v8::Value> maybe_result =
        function->Call(context, context->Global(), argc, argv);
    v8::Global<v8::Value> result;
    v8::Local<v8::Value> local;
    if (maybe_result.ToLocal(&local))
      result.Reset(context->GetIsolate(), local);
    return result;
  };

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      base::Bind(run_js_and_allow_error));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setUpdateArgumentsPreValidate('oneString', function() {\n"
      "    throw new Error('Custom Hook Error');\n"
      "  });\n"
      "})";
  v8::Local<v8::String> source_string =
      gin::StringToV8(isolate(), kRegisterHook);
  v8::Local<v8::String> source_name =
      gin::StringToV8(isolate(), "custom_hook");
  hooks->RegisterJsSource(
      v8::Global<v8::String>(isolate(), source_string),
      v8::Global<v8::String>(isolate(), source_name));

  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;

  APIBinding binding(
      "test", functions.get(), nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      std::move(hooks), &refs);
  EXPECT_TRUE(refs.empty());

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                            arraysize(args), args,
                            "Uncaught Error: Custom Hook Error");

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
}

}  // namespace extensions
