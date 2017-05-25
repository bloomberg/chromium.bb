// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_hooks.h"
#include "extensions/renderer/api_binding_hooks_test_delegate.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_invocation_errors.h"
#include "extensions/renderer/api_request_handler.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "v8/include/v8.h"

namespace extensions {

using namespace api_errors;

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
    "  'name': 'intAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer'"
    "  }, {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }]"
    "}]";

bool AllowAllFeatures(v8::Local<v8::Context> context, const std::string& name) {
  return true;
}

void OnEventListenersChanged(const std::string& event_name,
                             binding::EventListenersChanged change,
                             const base::DictionaryValue* filter,
                             bool was_manual,
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
        APILastError(APILastError::GetParent(),
                     APILastError::AddConsoleError()));
  }

  void TearDown() override {
    DisposeAllContexts();
    request_handler_.reset();
    event_handler_.reset();
    binding_.reset();
    APIBindingTest::TearDown();
  }

  void OnWillDisposeContext(v8::Local<v8::Context> context) override {
    event_handler_->InvalidateContext(context);
    request_handler_->InvalidateContext(context);
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

  void SetHooksDelegate(
      std::unique_ptr<APIBindingHooksDelegate> hooks_delegate) {
    binding_hooks_delegate_ = std::move(hooks_delegate);
    ASSERT_TRUE(binding_hooks_delegate_);
  }

  void SetCreateCustomType(const APIBinding::CreateCustomType& callback) {
    create_custom_type_ = callback;
  }

  void SetAvailabilityCallback(
      const APIBinding::AvailabilityCallback& callback) {
    availability_callback_ = callback;
  }

  void InitializeBinding() {
    if (!binding_hooks_) {
      binding_hooks_ = base::MakeUnique<APIBindingHooks>(
          kBindingName, binding::RunJSFunctionSync());
    }
    if (binding_hooks_delegate_)
      binding_hooks_->SetDelegate(std::move(binding_hooks_delegate_));
    if (!availability_callback_)
      availability_callback_ = base::Bind(&AllowAllFeatures);
    event_handler_ = base::MakeUnique<APIEventHandler>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult),
        base::Bind(&OnEventListenersChanged));
    binding_ = base::MakeUnique<APIBinding>(
        kBindingName, binding_functions_.get(), binding_types_.get(),
        binding_events_.get(), binding_properties_.get(), create_custom_type_,
        availability_callback_, std::move(binding_hooks_), &type_refs_,
        request_handler_.get(), event_handler_.get());
    EXPECT_EQ(!binding_types_.get(), type_refs_.empty());
  }

  void ExpectPass(v8::Local<v8::Object> object,
                  const std::string& script_source,
                  const std::string& expected_json_arguments_single_quotes,
                  bool expect_callback) {
    ExpectPass(MainContext(), object, script_source,
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
    RunTest(MainContext(), object, script_source, false, std::string(), false,
            "Uncaught TypeError: " + expected_error);
  }

  void ExpectThrow(v8::Local<v8::Object> object,
                   const std::string& script_source,
                   const std::string& expected_error) {
    RunTest(MainContext(), object, script_source, false, std::string(), false,
            "Uncaught Error: " + expected_error);
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
  std::unique_ptr<APIBindingHooksDelegate> binding_hooks_delegate_;
  APIBinding::CreateCustomType create_custom_type_;
  APIBinding::AvailabilityCallback availability_callback_;

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
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
  EXPECT_EQ(
      0u,
      binding_object->GetOwnPropertyNames(context).ToLocalChecked()->Length());
}

// Tests the basic call -> request flow of the API binding (ensuring that
// functions are set up correctly and correctly enforced).
TEST_F(APIBindingUnittest, TestBasicAPICalls) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Argument parsing is tested primarily in APISignature and ArgumentSpec
  // tests, so do a few quick sanity checks...
  ExpectPass(binding_object, "obj.oneString('foo');", "['foo']", false);
  ExpectFailure(
      binding_object, "obj.oneString(1);",
      InvocationError(
          "test.oneString", "string str",
          ArgumentError("str", InvalidType(kTypeString, kTypeInteger))));
  ExpectPass(binding_object, "obj.stringAndInt('foo', 1)", "['foo',1]", false);
  ExpectFailure(
      binding_object, "obj.stringAndInt(1)",
      InvocationError(
          "test.stringAndInt", "string str, integer int",
          ArgumentError("str", InvalidType(kTypeString, kTypeInteger))));
  ExpectPass(binding_object, "obj.intAndCallback(1, function() {})", "[1]",
             true);
  ExpectFailure(
      binding_object, "obj.intAndCallback(function() {})",
      InvocationError(
          "test.intAndCallback", "integer int, function callback",
          ArgumentError("int", InvalidType(kTypeInteger, kTypeFunction))));

  // ...And an interesting case (throwing an error during parsing).
  ExpectThrow(binding_object,
              "obj.oneObject({ get prop1() { throw new Error('Badness'); } });",
              "Badness");
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
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  const char kExpected[] =
      "{'ALPHA':'alpha','CAMEL_CASE':'camelCase','HYPHEN_ATED':'Hyphen-ated',"
      "'NUMS123':'nums123','SCREAMING':'SCREAMING','_42NUMS':'42nums'}";
  EXPECT_EQ(ReplaceSingleQuotes(kExpected),
            GetStringPropertyFromObject(binding_object, context, "first"));
  EXPECT_EQ(ReplaceSingleQuotes("{'OMEGA':'omega'}"),
            GetStringPropertyFromObject(binding_object, context, "last"));
}

// Test that empty enum entries are (unfortunately) allowed.
TEST_F(APIBindingUnittest, EnumWithEmptyEntry) {
  const char kTypes[] =
      "[{"
      "  'id': 'enumWithEmpty',"
      "  'type': 'string',"
      "  'enum': [{'name': ''}, {'name': 'other'}]"
      "}]";

  SetTypes(kTypes);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  EXPECT_EQ(
      "{\"\":\"\",\"OTHER\":\"other\"}",
      GetStringPropertyFromObject(binding_object, context, "enumWithEmpty"));
}

// Test that type references are correctly set up in the API.
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
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Parsing in general is tested in APISignature and ArgumentSpec tests, but
  // we test that the binding a) correctly finds the definitions, and b) accepts
  // properties from the API object.
  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo'})",
             "[{'prop1':'foo'}]", false);
  ExpectFailure(
      binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 'a'})",
      InvocationError(
          "test.takesRefObj", "refObj o",
          ArgumentError(
              "o",
              PropertyError("prop2", InvalidType(kTypeInteger, kTypeString)))));
  ExpectPass(binding_object, "obj.takesRefEnum('alpha')", "['alpha']", false);
  ExpectPass(binding_object, "obj.takesRefEnum(obj.refEnum.BETA)", "['beta']",
             false);
  ExpectFailure(
      binding_object, "obj.takesRefEnum('gamma')",
      InvocationError("test.takesRefEnum", "refEnum e",
                      ArgumentError("e", InvalidEnumValue({"alpha", "beta"}))));
}

TEST_F(APIBindingUnittest, RestrictedAPIs) {
  const char kFunctions[] =
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
  SetFunctions(kFunctions);
  const char kEvents[] =
      "[{'name': 'allowedEvent'}, {'name': 'restrictedEvent'}]";
  SetEvents(kEvents);
  auto is_available = [](v8::Local<v8::Context> context,
                         const std::string& name) {
    std::set<std::string> allowed = {"test.allowedOne", "test.allowedTwo",
                                     "test.allowedEvent"};
    std::set<std::string> restricted = {
        "test.restrictedOne", "test.restrictedTwo", "test.restrictedEvent"};
    EXPECT_TRUE(allowed.count(name) || restricted.count(name)) << name;
    return allowed.count(name) != 0;
  };
  SetAvailabilityCallback(base::Bind(is_available));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  auto is_defined = [&binding_object, context](const std::string& name) {
    v8::Local<v8::Value> val =
        GetPropertyFromObject(binding_object, context, name);
    EXPECT_FALSE(val.IsEmpty());
    return !val->IsUndefined() && !val->IsNull();
  };

  EXPECT_TRUE(is_defined("allowedOne"));
  EXPECT_TRUE(is_defined("allowedTwo"));
  EXPECT_TRUE(is_defined("allowedEvent"));
  EXPECT_FALSE(is_defined("restrictedOne"));
  EXPECT_FALSE(is_defined("restrictedTwo"));
  EXPECT_FALSE(is_defined("restrictedEvent"));
}

// Tests that events specified in the API are created as properties of the API
// object.
TEST_F(APIBindingUnittest, TestEventCreation) {
  SetEvents(
      R"([
           {'name': 'onFoo'},
           {'name': 'onBar'},
           {'name': 'onBaz', 'options': {'maxListeners': 1}}
         ])");
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  EXPECT_TRUE(has_on_baz.FromJust());

  // Test that the maxListeners property is correctly used.
  v8::Local<v8::Function> add_listener = FunctionFromString(
      context, "(function(e) { e.addListener(function() {}); })");
  v8::Local<v8::Value> args[] = {
      GetPropertyFromObject(binding_object, context, "onBaz")};
  RunFunction(add_listener, context, arraysize(args), args);
  EXPECT_EQ(1u, event_handler()->GetNumEventListenersForTesting("test.onBaz",
                                                                context));
  RunFunctionAndExpectError(add_listener, context, arraysize(args), args,
                            "Uncaught TypeError: Too many listeners.");
  EXPECT_EQ(1u, event_handler()->GetNumEventListenersForTesting("test.onBaz",
                                                                context));

  v8::Maybe<bool> has_nonexistent_event = binding_object->Has(
      context, gin::StringToV8(isolate(), "onNonexistentEvent"));
  EXPECT_TRUE(has_nonexistent_event.IsJust());
  EXPECT_FALSE(has_nonexistent_event.FromJust());
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
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
  EXPECT_EQ("17",
            GetStringPropertyFromObject(binding_object, context, "prop1"));
  EXPECT_EQ(R"({"subprop1":"some value","subprop2":true})",
            GetStringPropertyFromObject(binding_object, context, "prop2"));
}

TEST_F(APIBindingUnittest, TestRefProperties) {
  SetProperties(
      "{"
      "  'alpha': {"
      "    '$ref': 'AlphaRef',"
      "    'value': ['a']"
      "  },"
      "  'beta': {"
      "    '$ref': 'BetaRef',"
      "    'value': ['b']"
      "  }"
      "}");
  auto create_custom_type = [](v8::Isolate* isolate,
                               const std::string& type_name,
                               const std::string& property_name,
                               const base::ListValue* property_values) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    if (type_name == "AlphaRef") {
      EXPECT_EQ("alpha", property_name);
      EXPECT_EQ("[\"a\"]", ValueToString(*property_values));
      result
          ->Set(context, gin::StringToSymbol(isolate, "alphaProp"),
                gin::StringToV8(isolate, "alphaVal"))
          .ToChecked();
    } else if (type_name == "BetaRef") {
      EXPECT_EQ("beta", property_name);
      EXPECT_EQ("[\"b\"]", ValueToString(*property_values));
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
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);
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
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  v8::Local<v8::Function> func =
      FunctionFromString(context, "(function(obj) { obj.oneString('foo'); })");
  v8::Local<v8::Value> argv[] = {binding_object};
  DisposeContext(context);
  RunFunction(func, context, arraysize(argv), argv);
  EXPECT_FALSE(HandlerWasInvoked());
  // This test passes if this does not crash, even under AddressSanitizer
  // builds.
}

TEST_F(APIBindingUnittest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object_a = binding()->CreateInstance(context_a);
  v8::Local<v8::Object> binding_object_b = binding()->CreateInstance(context_b);

  ExpectPass(context_a, binding_object_a, "obj.oneString('foo');", "['foo']",
             false);
  ExpectPass(context_b, binding_object_b, "obj.oneString('foo');", "['foo']",
             false);
  DisposeContext(context_a);
  ExpectPass(context_b, binding_object_b, "obj.oneString('foo');", "['foo']",
             false);
}

// Tests adding custom hooks for an API method.
TEST_F(APIBindingUnittest, TestCustomHooks) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooksTestDelegate>();
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
  hooks->AddHandler("test.oneString", base::Bind(hook, &did_call));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setHandleRequest('oneString', function() {\n"
        "    this.requestArguments = Array.from(arguments);\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetFunctions(kFunctions);
  SetHooks(std::move(hooks));
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // First try calling with an invalid invocation. An error should be raised and
  // the hook should never have been called, since the arguments didn't match.
  ExpectFailure(
      binding_object, "obj.oneString(1);",
      InvocationError(
          "test.oneString", "string str",
          ArgumentError("str", InvalidType(kTypeString, kTypeInteger))));
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
  v8::Local<v8::Context> context = MainContext();
  const char kRegisterHook[] =
      "(function(hooks) {\n"
      "  hooks.setUpdateArgumentsPreValidate('oneString', function() {\n"
      "    this.requestArguments = Array.from(arguments);\n"
      "    if (this.requestArguments[0] === true)\n"
      "      return ['hooked']\n"
      "    return this.requestArguments\n"
      "  });\n"
      "})";
  v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
  v8::Local<v8::Function> function = FunctionFromString(context, kRegisterHook);
  v8::Local<v8::Value> args[] = {js_hooks};
  RunFunctionOnGlobal(function, context, arraysize(args), args);

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Call the method with a hook. Since the hook updates arguments before
  // validation, we should be able to pass in invalid arguments and still
  // have the hook called.
  ExpectFailure(
      binding_object, "obj.oneString(false);",
      InvocationError(
          "test.oneString", "string str",
          ArgumentError("str", InvalidType(kTypeString, kTypeBoolean))));
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
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setUpdateArgumentsPreValidate('oneString', function() {\n"
        "    throw new Error('Custom Hook Error');\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setHandleRequest('oneString', str => {\n"
        "    return str + ' pong';\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setHandleRequest('oneString', str => {\n"
        "    throw new Error('Custom Hook Error');\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  v8::Local<v8::Context> context = MainContext();

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooksTestDelegate>();
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
  hooks->AddHandler("test.oneString", base::Bind(hook, &did_call));

  SetHooksDelegate(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setUpdateArgumentsPostValidate('oneString', function() {\n"
        "    this.requestArguments = Array.from(arguments);\n"
        "    return ['pong'];\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Try calling the method with an invalid signature. Since it's invalid, we
  // should never enter the hook.
  ExpectFailure(
      binding_object, "obj.oneString(false);",
      InvocationError(
          "test.oneString", "string str",
          ArgumentError("str", InvalidType(kTypeString, kTypeBoolean))));
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

// Tests using setUpdateArgumentsPostValidate to return a list of arguments
// that violates the function schema. Sadly, this should succeed. :(
// See comment in api_binding.cc.
TEST_F(APIBindingUnittest, TestUpdateArgumentsPostValidateViolatingSchema) {
  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooks>(
      kBindingName, base::Bind(&RunFunctionOnGlobalAndReturnHandle));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  {
    const char kRegisterHook[] =
        "(function(hooks) {\n"
        "  hooks.setUpdateArgumentsPostValidate('oneString', function() {\n"
        "    return [{}];\n"
        "  });\n"
        "})";
    v8::Local<v8::Object> js_hooks = hooks->GetJSHookInterface(context);
    v8::Local<v8::Function> function =
        FunctionFromString(context, kRegisterHook);
    v8::Local<v8::Value> args[] = {js_hooks};
    RunFunctionOnGlobal(function, context, arraysize(args), args);
  }

  SetHooks(std::move(hooks));
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // Call the method with a valid signature. The hook should be entered and
  // manipulate the arguments.
  ExpectPass(binding_object, "obj.oneString('ping');", "[{}]", false);
}

// Test that user gestures are properly recorded when calling APIs.
TEST_F(APIBindingUnittest, TestUserGestures) {
  SetFunctions(kFunctions);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

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

TEST_F(APIBindingUnittest, FilteredEvents) {
  const char kEvents[] =
      "[{"
      "  'name': 'unfilteredOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'unfilteredTwo',"
      "  'filters': [],"
      "  'parameters': []"
      "}, {"
      "  'name': 'unfilteredThree',"
      "  'options': {'supportsFilters': false},"
      "  'parameters': []"
      "}, {"
      "  'name': 'filteredOne',"
      "  'options': {'supportsFilters': true},"
      "  'parameters': []"
      "}, {"
      "  'name': 'filteredTwo',"
      "  'filters': ["
      "    {'name': 'url', 'type': 'array', 'items': {'type': 'any'}}"
      "  ],"
      "  'parameters': []"
      "}]";
  SetEvents(kEvents);
  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  const char kAddFilteredListener[] =
      "(function(evt) {\n"
      "  evt.addListener(function() {},\n"
      "                  {url: [{pathContains: 'simple2.html'}]});\n"
      "})";
  v8::Local<v8::Function> function =
      FunctionFromString(context, kAddFilteredListener);
  ASSERT_FALSE(function.IsEmpty());

  auto check_supports_filters = [context, binding_object, function](
                                    base::StringPiece name,
                                    bool expect_supports) {
    SCOPED_TRACE(name);
    v8::Local<v8::Value> event =
        GetPropertyFromObject(binding_object, context, name);
    v8::Local<v8::Value> args[] = {event};
    if (expect_supports) {
      RunFunction(function, context, context->Global(), arraysize(args), args);
    } else {
      RunFunctionAndExpectError(
          function, context, context->Global(), arraysize(args), args,
          "Uncaught TypeError: This event does not support filters");
    }
  };

  check_supports_filters("unfilteredOne", false);
  check_supports_filters("unfilteredTwo", false);
  check_supports_filters("unfilteredThree", false);
  check_supports_filters("filteredOne", true);
  check_supports_filters("filteredTwo", true);
}

TEST_F(APIBindingUnittest, HooksTemplateInitializer) {
  SetFunctions(kFunctions);

  // Register a hook for the test.oneString method.
  auto hooks = base::MakeUnique<APIBindingHooksTestDelegate>();
  auto hook = [](v8::Isolate* isolate,
                 v8::Local<v8::ObjectTemplate> object_template,
                 const APITypeReferenceMap& type_refs) {
    object_template->Set(gin::StringToSymbol(isolate, "hookedProperty"),
                         gin::ConvertToV8(isolate, 42));
  };
  hooks->SetTemplateInitializer(base::Bind(hook));
  SetHooksDelegate(std::move(hooks));

  InitializeBinding();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Object> binding_object = binding()->CreateInstance(context);

  // The extra property should be present on the binding object.
  EXPECT_EQ("42", GetStringPropertyFromObject(binding_object, context,
                                              "hookedProperty"));
  // Sanity check: other values should still be there.
  EXPECT_EQ("function",
            GetStringPropertyFromObject(binding_object, context, "oneString"));
}

}  // namespace extensions
