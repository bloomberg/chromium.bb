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
#include "extensions/renderer/api_type_reference_map.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

const char kBindingName[] = "test";

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
    "}, {"
    "  'name': 'intAnyOptionalObjectOptionalCallback',"
    "  'parameters': [{"
    "    'type': 'integer', 'name': 'tabId', 'minimum': 0"
    "  }, {"
    "    'type': 'any', 'name': 'message'"
    "  }, {"
    "    'type': 'object',"
    "    'name': 'options',"
    "    'properties': {"
    "      'frameId': {'type': 'integer', 'optional': true, 'minimum': 0}"
    "    },"
    "    'optional': true"
    "  }, {"
    "    'type': 'function', 'name': 'responseCallback', 'optional': true"
    "  }]"
    "}]";

const char kError[] = "Uncaught TypeError: Invalid invocation";

bool AllowAllAPIs(const std::string& name) {
  return true;
}

void OnEventListenersChanged(const std::string& event_name,
                             binding::EventListenersChanged change,
                             v8::Local<v8::Context> context) {}

}  // namespace

class APIBindingUnittest : public APIBindingTest {
 public:
  void OnFunctionCall(std::unique_ptr<APIRequestHandler::Request> request,
                      v8::Local<v8::Context> context) {
    last_request_ = std::move(request);
  }

 protected:
  APIBindingUnittest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()) {}
  void SetUp() override {
    APIBindingTest::SetUp();
    request_handler_ = base::MakeUnique<APIRequestHandler>(
        base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult),
        APILastError(APILastError::GetParent()));
  }

  void TearDown() override {
    request_handler_.reset();
    event_handler_.reset();
    binding_.reset();
    APIBindingTest::TearDown();
  }

  void SetFunctions(const char* functions) {
    binding_functions_ = ListValueFromString(functions);
    ASSERT_TRUE(binding_functions_);
  }

  void SetEvents(const char* events) {
    binding_events_ = ListValueFromString(events);
    ASSERT_TRUE(binding_events_);
  }

  void SetTypes(const char* types) {
    binding_types_ = ListValueFromString(types);
    ASSERT_TRUE(binding_types_);
  }

  void SetProperties(const char* properties) {
    binding_properties_ = DictionaryValueFromString(properties);
    ASSERT_TRUE(binding_properties_);
  }

  void SetHooks(std::unique_ptr<APIBindingHooks> hooks) {
    binding_hooks_ = std::move(hooks);
    ASSERT_TRUE(binding_hooks_);
  }

  void SetCreateCustomType(const APIBinding::CreateCustomType& callback) {
    create_custom_type_ = callback;
  }

  void InitializeBinding() {
    if (!binding_hooks_) {
      binding_hooks_ = base::MakeUnique<APIBindingHooks>(
          kBindingName, binding::RunJSFunctionSync());
    }
    event_handler_ = base::MakeUnique<APIEventHandler>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult),
        base::Bind(&OnEventListenersChanged));
    binding_ = base::MakeUnique<APIBinding>(
        kBindingName, binding_functions_.get(), binding_types_.get(),
        binding_events_.get(), binding_properties_.get(), create_custom_type_,
        std::move(binding_hooks_), &type_refs_, request_handler_.get(),
        event_handler_.get());
    EXPECT_EQ(!binding_types_.get(), type_refs_.empty());
  }

  void ExpectPass(v8::Local<v8::Object> object,
                  const std::string& script_source,
                  const std::string& expected_json_arguments_single_quotes,
                  bool expect_callback) {
    ExpectPass(ContextLocal(), object, script_source,
               expected_json_arguments_single_quotes, expect_callback);
  }

  void ExpectPass(v8::Local<v8::Context> context,
                  v8::Local<v8::Object> object,
                  const std::string& script_source,
                  const std::string& expected_json_arguments_single_quotes,
                  bool expect_callback) {
    RunTest(context, object, script_source, true,
            ReplaceSingleQuotes(expected_json_arguments_single_quotes),
            expect_callback, std::string());
  }

  void ExpectFailure(v8::Local<v8::Object> object,
                     const std::string& script_source,
                     const std::string& expected_error) {
    RunTest(ContextLocal(), object, script_source, false, std::string(), false,
            expected_error);
  }

  bool HandlerWasInvoked() const { return last_request_ != nullptr; }
  const APIRequestHandler::Request* last_request() const {
    return last_request_.get();
  }
  void reset_last_request() { last_request_.reset(); }
  APIBinding* binding() { return binding_.get(); }
  APIEventHandler* event_handler() { return event_handler_.get(); }
  APIRequestHandler* request_handler() { return request_handler_.get(); }
  const APITypeReferenceMap& type_refs() const { return type_refs_; }

 private:
  void RunTest(v8::Local<v8::Context> context,
               v8::Local<v8::Object> object,
               const std::string& script_source,
               bool should_pass,
               const std::string& expected_json_arguments,
               bool expect_callback,
               const std::string& expected_error);

  std::unique_ptr<APIRequestHandler::Request> last_request_;
  std::unique_ptr<APIBinding> binding_;
  std::unique_ptr<APIEventHandler> event_handler_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  APITypeReferenceMap type_refs_;

  std::unique_ptr<base::ListValue> binding_functions_;
  std::unique_ptr<base::ListValue> binding_events_;
  std::unique_ptr<base::ListValue> binding_types_;
  std::unique_ptr<base::DictionaryValue> binding_properties_;
  std::unique_ptr<APIBindingHooks> binding_hooks_;
  APIBinding::CreateCustomType create_custom_type_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingUnittest);
};

void APIBindingUnittest::RunTest(v8::Local<v8::Context> context,
                                 v8::Local<v8::Object> object,
                                 const std::string& script_source,
                                 bool should_pass,
                                 const std::string& expected_json_arguments,
                                 bool expect_callback,
                                 const std::string& expected_error) {
  EXPECT_FALSE(last_request_);
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  ASSERT_FALSE(func.IsEmpty());

  v8::Local<v8::Value> argv[] = {object};

  if (should_pass) {
    RunFunction(func, context, 1, argv);
    ASSERT_TRUE(last_request_) << script_source;
    EXPECT_EQ(expected_json_arguments,
              ValueToString(*last_request_->arguments));
    EXPECT_EQ(expect_callback, last_request_->has_callback) << script_source;
  } else {
    RunFunctionAndExpectError(func, context, 1, argv, expected_error);
    EXPECT_FALSE(last_request_);
  }

  last_request_.reset();
}

TEST_F(APIBindingUnittest, TestEmptyAPI) {
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));
  EXPECT_EQ(
      0u,
      binding_object->GetOwnPropertyNames(context).ToLocalChecked()->Length());
}

TEST_F(APIBindingUnittest, Test) {
  // TODO(devlin): Move this test to an api_signature_unittest file? It really
  // only tests parsing.
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  ExpectPass(binding_object, "obj.oneString('foo');", "['foo']", false);
  ExpectPass(binding_object, "obj.oneString('');", "['']", false);
  ExpectFailure(binding_object, "obj.oneString(1);", kError);
  ExpectFailure(binding_object, "obj.oneString();", kError);
  ExpectFailure(binding_object, "obj.oneString({});", kError);
  ExpectFailure(binding_object, "obj.oneString('foo', 'bar');", kError);

  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
  ExpectPass(binding_object, "obj.stringAndInt('foo', -1);", "['foo',-1]",
             false);
  ExpectFailure(binding_object, "obj.stringAndInt(1);", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt(1, 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', '1');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 2.3);", kError);

  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', 42, true);",
             "['foo',42,true]", false);
  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', true);",
             "['foo',null,true]", false);
  ExpectFailure(binding_object,
                "obj.stringOptionalIntAndBool('foo', 'bar', true);", kError);

  ExpectPass(binding_object, "obj.oneObject({prop1: 'foo'});",
             "[{'prop1':'foo'}]", false);
  ExpectFailure(
      binding_object,
      "obj.oneObject({ get prop1() { throw new Error('Badness'); } });",
      "Uncaught Error: Badness");

  ExpectPass(binding_object, "obj.noArgs()", "[]", false);
  ExpectFailure(binding_object, "obj.noArgs(0)", kError);
  ExpectFailure(binding_object, "obj.noArgs('')", kError);
  ExpectFailure(binding_object, "obj.noArgs(null)", kError);
  ExpectFailure(binding_object, "obj.noArgs(undefined)", kError);

  ExpectPass(binding_object, "obj.intAndCallback(1, function() {})", "[1]",
             true);
  ExpectFailure(binding_object, "obj.intAndCallback(function() {})", kError);
  ExpectFailure(binding_object, "obj.intAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalIntAndCallback(1, function() {})",
             "[1]", true);
  ExpectPass(binding_object, "obj.optionalIntAndCallback(function() {})",
             "[null]", true);
  ExpectFailure(binding_object, "obj.optionalIntAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalCallback(function() {})", "[]", true);
  ExpectPass(binding_object, "obj.optionalCallback()", "[]", false);
  ExpectPass(binding_object, "obj.optionalCallback(undefined)", "[]", false);
  ExpectFailure(binding_object, "obj.optionalCallback(0)", kError);

  ExpectPass(binding_object,
             "obj.intAnyOptionalObjectOptionalCallback(4, {foo: 'bar'}, "
             "function() {})",
             "[4,{'foo':'bar'},null]", true);
  ExpectPass(binding_object,
             "obj.intAnyOptionalObjectOptionalCallback(4, {foo: 'bar'})",
             "[4,{'foo':'bar'},null]", false);
  ExpectPass(binding_object,
             "obj.intAnyOptionalObjectOptionalCallback(4, {foo: 'bar'}, {})",
             "[4,{'foo':'bar'},{}]", false);
  ExpectFailure(binding_object,
                "obj.intAnyOptionalObjectOptionalCallback(4, function() {})",
                kError);
  ExpectFailure(binding_object, "obj.intAnyOptionalObjectOptionalCallback(4)",
                kError);
}

// Test that enum values are properly exposed on the binding object.
TEST_F(APIBindingUnittest, EnumValues) {
  const char kTypes[] =
      "[{"
      "  'id': 'first',"
      "  'type': 'string',"
      "  'enum': ['alpha', 'camelCase', 'Hyphen-ated',"
      "           'SCREAMING', 'nums123', '42nums']"
      "}, {"
      "  'id': 'last',"
      "  'type': 'string',"
      "  'enum': [{'name': 'omega'}]"
      "}]";

  SetTypes(kTypes);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  const char kExpected[] =
      "{'ALPHA':'alpha','CAMEL_CASE':'camelCase','HYPHEN_ATED':'Hyphen-ated',"
      "'NUMS123':'nums123','SCREAMING':'SCREAMING','_42NUMS':'42nums'}";
  EXPECT_EQ(ReplaceSingleQuotes(kExpected),
            GetStringPropertyFromObject(binding_object, context, "first"));
  EXPECT_EQ(ReplaceSingleQuotes("{'OMEGA':'omega'}"),
            GetStringPropertyFromObject(binding_object, context, "last"));
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

  SetFunctions(kRefFunctions);
  SetTypes(kTypes);
  InitializeBinding();
  EXPECT_EQ(2u, type_refs().size());
  EXPECT_TRUE(type_refs().GetSpec("refObj"));
  EXPECT_TRUE(type_refs().GetSpec("refEnum"));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo'})",
             "[{'prop1':'foo'}]", false);
  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 2})",
             "[{'prop1':'foo','prop2':2}]", false);
  ExpectFailure(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 'a'})",
                kError);
  ExpectPass(binding_object, "obj.takesRefEnum('alpha')", "['alpha']", false);
  ExpectPass(binding_object, "obj.takesRefEnum('beta')", "['beta']", false);
  ExpectPass(binding_object, "obj.takesRefEnum(obj.refEnum.BETA)", "['beta']",
             false);
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
  SetFunctions(kRestrictedFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  auto is_available = [](const std::string& name) {
    std::set<std::string> functions = {"test.allowedOne", "test.allowedTwo",
                                       "test.restrictedOne",
                                       "test.restrictedTwo"};
    EXPECT_TRUE(functions.count(name));
    return name == "test.allowedOne" || name == "test.allowedTwo";
  };

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(is_available));

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
  SetEvents("[{'name': 'onFoo'}, {'name': 'onBar'}]");
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

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

TEST_F(APIBindingUnittest, TestProperties) {
  SetProperties(
      "{"
      "  'prop1': { 'value': 17, 'type': 'integer' },"
      "  'prop2': {"
      "    'type': 'object',"
      "    'properties': {"
      "      'subprop1': { 'value': 'some value', 'type': 'string' },"
      "      'subprop2': { 'value': true, 'type': 'boolean' }"
      "    }"
      "  }"
      "}");
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));
  EXPECT_EQ("17",
            GetStringPropertyFromObject(binding_object, context, "prop1"));
  EXPECT_EQ(R"({"subprop1":"some value","subprop2":true})",
            GetStringPropertyFromObject(binding_object, context, "prop2"));
}

TEST_F(APIBindingUnittest, TestRefProperties) {
  SetProperties(
      "{"
      "  'alpha': {"
      "    '$ref': 'AlphaRef'"
      "  },"
      "  'beta': {"
      "    '$ref': 'BetaRef'"
      "  }"
      "}");
  auto create_custom_type = [](v8::Local<v8::Context> context,
                               const std::string& type_name,
                               const std::string& property_name) {
    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    if (type_name == "AlphaRef") {
      EXPECT_EQ("alpha", property_name);
      result
          ->Set(context, gin::StringToSymbol(isolate, "alphaProp"),
                gin::StringToV8(isolate, "alphaVal"))
          .ToChecked();
    } else if (type_name == "BetaRef") {
      EXPECT_EQ("beta", property_name);
      result
          ->Set(context, gin::StringToSymbol(isolate, "betaProp"),
                gin::StringToV8(isolate, "betaVal"))
          .ToChecked();
    } else {
      EXPECT_TRUE(false) << type_name;
    }
    return result;
  };

  SetCreateCustomType(base::Bind(create_custom_type));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));
  EXPECT_EQ(R"({"alphaProp":"alphaVal"})",
            GetStringPropertyFromObject(binding_object, context, "alpha"));
  EXPECT_EQ(
      R"({"betaProp":"betaVal"})",
      GetStringPropertyFromObject(binding_object, context, "beta"));
}

TEST_F(APIBindingUnittest, TestDisposedContext) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> argv[] = {binding_object};
  DisposeContext();
  RunFunction(func, context, arraysize(argv), argv);
  EXPECT_FALSE(HandlerWasInvoked());
  // This test passes if this does not crash, even under AddressSanitizer
  // builds.
}

TEST_F(APIBindingUnittest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = ContextLocal();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate());
  gin::ContextHolder holder_b(isolate());
  holder_b.SetContext(context_b);

  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object_a = binding()->CreateInstance(
      context_a, isolate(), base::Bind(&AllowAllAPIs));
  v8::Local<v8::Object> binding_object_b = binding()->CreateInstance(
      context_b, isolate(), base::Bind(&AllowAllAPIs));

  ExpectPass(context_a, binding_object_a, "obj.oneString('foo');", "['foo']",
             false);
  ExpectPass(context_b, binding_object_b, "obj.oneString('foo');", "['foo']",
             false);
  DisposeContext();
  ExpectPass(context_b, binding_object_b, "obj.oneString('foo');", "['foo']",
             false);
}

// Tests adding custom hooks for an API method.
TEST_F(APIBindingUnittest, TestCustomHooks) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));
  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 std::vector<v8::Local<v8::Value>>* arguments,
                 const APITypeReferenceMap& ref_map) {
    *did_call = true;
    APIBindingHooks::RequestResult result(
        APIBindingHooks::RequestResult::HANDLED);
    if (arguments->size() != 1u) {  // ASSERT* messes with the return type.
      EXPECT_EQ(1u, arguments->size());
      return result;
    }
    EXPECT_EQ("foo", gin::V8ToString(arguments->at(0)));
    return result;
  };
  hooks->RegisterHandleRequest("test.oneString", base::Bind(hook, &did_call));
  SetHooks(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  // First try calling the oneString() method, which has a custom hook
  // installed.
  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunction(func, context, 1, args);
  EXPECT_TRUE(did_call);

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

TEST_F(APIBindingUnittest, TestJSCustomHook) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));

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

  SetFunctions(kFunctions);
  SetHooks(std::move(hooks));
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

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

  EXPECT_EQ("[\"foo\"]", GetStringPropertyFromObject(
                             context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

// Tests the updateArgumentsPreValidate hook.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPreValidate) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));

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

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  // Call the method with a hook. Since the hook updates arguments before
  // validation, we should be able to pass in invalid arguments and still
  // have the hook called.
  ExpectFailure(binding_object, "obj.oneString(false);", kError);
  EXPECT_EQ("[false]", GetStringPropertyFromObject(
                           context->Global(), context, "requestArguments"));

  ExpectPass(binding_object, "obj.oneString(true);", "['hooked']", false);
  EXPECT_EQ("[true]", GetStringPropertyFromObject(
                          context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
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
      kBindingName, base::Bind(run_js_and_allow_error));

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

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                            arraysize(args), args,
                            "Uncaught Error: Custom Hook Error");

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]",
             false);
}

// Tests that custom JS hooks can return results synchronously.
TEST_F(APIBindingUnittest, TestReturningResultFromCustomJSHook) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setHandleRequest('oneString', str => {\n"
      "    return str + ' pong';\n"
      "  });\n"
      "})";
  v8::Local<v8::String> source_string =
      gin::StringToV8(isolate(), kRegisterHook);
  v8::Local<v8::String> source_name =
      gin::StringToV8(isolate(), "custom_hook");
  hooks->RegisterJsSource(
      v8::Global<v8::String>(isolate(), source_string),
      v8::Global<v8::String>(isolate(), source_name));

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  v8::Local<v8::Value> result =
      RunFunction(function, context, arraysize(args), args);
  ASSERT_FALSE(result.IsEmpty());
  std::unique_ptr<base::Value> json_result = V8ToBaseValue(result, context);
  ASSERT_TRUE(json_result);
  EXPECT_EQ("\"ping pong\"", ValueToString(*json_result));
}

// Tests that JS custom hooks can throw exceptions for bad invocations.
TEST_F(APIBindingUnittest, TestThrowingFromCustomJSHook) {
  // Our testing handlers for running functions expect a pre-determined success
  // or failure. Since we're testing throwing exceptions here, we need a way of
  // running that allows exceptions to be thrown, but we still expect most JS
  // calls to succeed.
  // TODO(devlin): This is a bit clunky. If we need to do this enough, we could
  // figure out a different solution, like having a stack object for allowing
  // errors/exceptions. But given this is the only place we need it so far, this
  // is sufficient.
  auto run_js_and_expect_error = [](v8::Local<v8::Function> function,
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
      kBindingName, base::Bind(run_js_and_expect_error));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setHandleRequest('oneString', str => {\n"
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

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> function =
      FunctionFromString(context,
                         "(function(obj) { return obj.oneString('ping'); })");
  v8::Local<v8::Value> args[] = {binding_object};
  RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                            arraysize(args), args,
                            "Uncaught Error: Custom Hook Error");
}

// Tests that native custom hooks can return results synchronously, or throw
// exceptions for bad invocations.
TEST_F(APIBindingUnittest,
       TestReturningResultAndThrowingExceptionFromCustomNativeHook) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));
  bool did_call = false;
  auto hook = [](bool* did_call, const APISignature* signature,
                 v8::Local<v8::Context> context,
                 std::vector<v8::Local<v8::Value>>* arguments,
                 const APITypeReferenceMap& ref_map) {
    APIBindingHooks::RequestResult result(
        APIBindingHooks::RequestResult::HANDLED);
    if (arguments->size() != 1u) {  // ASSERT* messes with the return type.
      EXPECT_EQ(1u, arguments->size());
      return result;
    }
    v8::Isolate* isolate = context->GetIsolate();
    std::string arg_value = gin::V8ToString(arguments->at(0));
    if (arg_value == "throw") {
      isolate->ThrowException(v8::Exception::Error(
          gin::StringToV8(isolate, "Custom Hook Error")));
      result.code = APIBindingHooks::RequestResult::THROWN;
      return result;
    }
    result.return_value =
        gin::StringToV8(context->GetIsolate(), arg_value + " pong");
    return result;
  };
  hooks->RegisterHandleRequest("test.oneString", base::Bind(hook, &did_call));

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  {
    // Test an invocation that we expect to throw an exception.
    v8::Local<v8::Function> function =
        FunctionFromString(
            context, "(function(obj) { return obj.oneString('throw'); })");
    v8::Local<v8::Value> args[] = {binding_object};
    RunFunctionAndExpectError(function, context, v8::Undefined(isolate()),
                              arraysize(args), args,
                              "Uncaught Error: Custom Hook Error");
  }

  {
    // Test an invocation we expect to succeed.
    v8::Local<v8::Function> function =
        FunctionFromString(context,
                           "(function(obj) { return obj.oneString('ping'); })");
    v8::Local<v8::Value> args[] = {binding_object};
    v8::Local<v8::Value> result =
        RunFunction(function, context, arraysize(args), args);
    ASSERT_FALSE(result.IsEmpty());
    std::unique_ptr<base::Value> json_result = V8ToBaseValue(result, context);
    ASSERT_TRUE(json_result);
    EXPECT_EQ("\"ping pong\"", ValueToString(*json_result));
  }
}

// Tests the updateArgumentsPostValidate hook.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPostValidate) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setUpdateArgumentsPostValidate('oneString', function() {\n"
      "    this.requestArguments = Array.from(arguments);\n"
      "    return ['pong'];\n"
      "  });\n"
      "})";
  v8::Local<v8::String> source_string =
      gin::StringToV8(isolate(), kRegisterHook);
  v8::Local<v8::String> source_name =
      gin::StringToV8(isolate(), "custom_hook");
  hooks->RegisterJsSource(
      v8::Global<v8::String>(isolate(), source_string),
      v8::Global<v8::String>(isolate(), source_name));

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  // Try calling the method with an invalid signature. Since it's invalid, we
  // should never enter the hook.
  ExpectFailure(binding_object, "obj.oneString(false);", kError);
  EXPECT_EQ("undefined", GetStringPropertyFromObject(
                             context->Global(), context, "requestArguments"));

  // Call the method with a valid signature. The hook should be entered and
  // manipulate the arguments.
  ExpectPass(binding_object, "obj.oneString('ping');", "['pong']", false);
  EXPECT_EQ("[\"ping\"]", GetStringPropertyFromObject(
                              context->Global(), context, "requestArguments"));

  // Other methods, like stringAndInt(), should behave normally.
  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);",
             "['foo',42]", false);
}

// Test that user gestures are properly recorded when calling APIs.
TEST_F(APIBindingUnittest, TestUserGestures) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> binding_object =
      binding()->CreateInstance(context, isolate(), base::Bind(&AllowAllAPIs));

  v8::Local<v8::Function> function =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo');})");
  ASSERT_FALSE(function.IsEmpty());

  v8::Local<v8::Value> argv[] = {binding_object};
  RunFunction(function, context, arraysize(argv), argv);
  ASSERT_TRUE(last_request());
  EXPECT_FALSE(last_request()->has_user_gesture);
  reset_last_request();

  blink::WebScopedUserGesture user_gesture(nullptr);
  RunFunction(function, context, arraysize(argv), argv);
  ASSERT_TRUE(last_request());
  EXPECT_TRUE(last_request()->has_user_gesture);

  reset_last_request();
}

}  // namespace extensions
