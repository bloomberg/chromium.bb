// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/argument_spec.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_invocation_errors.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

class ArgumentSpecUnitTest : public gin::V8Test {
 protected:
  ArgumentSpecUnitTest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()) {}
  ~ArgumentSpecUnitTest() override {}

  enum class TestResult {
    PASS,
    FAIL,
    THROW,
  };

  struct RunTestParams {
    RunTestParams(const ArgumentSpec& spec,
                  base::StringPiece script_source,
                  TestResult result)
        : spec(spec), script_source(script_source), expected_result(result) {}

    const ArgumentSpec& spec;
    base::StringPiece script_source;
    TestResult expected_result;
    base::StringPiece expected_json;
    base::StringPiece expected_error;
    base::StringPiece expected_thrown_message;
    const base::Value* expected_value = nullptr;
    bool should_convert = true;
  };

  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const std::string& expected_json_single_quotes) {
    RunTestParams params(spec, script_source, TestResult::PASS);
    std::string expected_json =
        ReplaceSingleQuotes(expected_json_single_quotes);
    params.expected_json = expected_json;
    RunTest(params);
  }

  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const base::Value& expected_value) {
    RunTestParams params(spec, script_source, TestResult::PASS);
    params.expected_value = &expected_value;
    RunTest(params);
  }

  void ExpectSuccessWithNoConversion(const ArgumentSpec& spec,
                                     const std::string& script_source) {
    RunTestParams params(spec, script_source, TestResult::PASS);
    params.should_convert = false;
    RunTest(params);
  }

  void ExpectFailure(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const std::string& expected_error) {
    RunTestParams params(spec, script_source, TestResult::FAIL);
    params.expected_error = expected_error;
    RunTest(params);
  }

  void ExpectFailureWithNoConversion(const ArgumentSpec& spec,
                                     const std::string& script_source,
                                     const std::string& expected_error) {
    RunTestParams params(spec, script_source, TestResult::FAIL);
    params.should_convert = false;
    params.expected_error = expected_error;
    RunTest(params);
  }

  void ExpectThrow(const ArgumentSpec& spec,
                   const std::string& script_source,
                   const std::string& expected_thrown_message) {
    RunTestParams params(spec, script_source, TestResult::THROW);
    params.expected_thrown_message = expected_thrown_message;
    RunTest(params);
  }

  void AddTypeRef(const std::string& id, std::unique_ptr<ArgumentSpec> spec) {
    type_refs_.AddSpec(id, std::move(spec));
  }

 private:
  void RunTest(const RunTestParams& params);

  APITypeReferenceMap type_refs_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpecUnitTest);
};

void ArgumentSpecUnitTest::RunTest(const RunTestParams& params) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(instance_->isolate());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(instance_->isolate(), context_);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> val =
      V8ValueFromScriptSource(context, params.script_source);
  ASSERT_FALSE(val.IsEmpty()) << params.script_source;

  std::string error;
  std::unique_ptr<base::Value> out_value;
  bool did_succeed = params.spec.ParseArgument(
      context, val, type_refs_, params.should_convert ? &out_value : nullptr,
      &error);
  bool should_succeed = params.expected_result == TestResult::PASS;
  ASSERT_EQ(should_succeed, did_succeed)
      << params.script_source << ", " << error;
  ASSERT_EQ(did_succeed && params.should_convert, !!out_value);
  bool should_throw = params.expected_result == TestResult::THROW;
  ASSERT_EQ(should_throw, try_catch.HasCaught()) << params.script_source;

  if (!params.expected_error.empty())
    EXPECT_EQ(params.expected_error, error) << params.script_source;

  if (should_succeed && params.should_convert) {
    ASSERT_TRUE(out_value);
    if (params.expected_value)
      EXPECT_TRUE(params.expected_value->Equals(out_value.get()))
          << params.script_source;
    else
      EXPECT_EQ(params.expected_json, ValueToString(*out_value));
  } else if (should_throw) {
    EXPECT_EQ(params.expected_thrown_message,
              gin::V8ToString(try_catch.Message()->Get()));
  }
}

TEST_F(ArgumentSpecUnitTest, Test) {
  using namespace api_errors;

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer'}"));
    ExpectSuccess(spec, "1", "1");
    ExpectSuccess(spec, "-1", "-1");
    ExpectSuccess(spec, "0", "0");
    ExpectSuccess(spec, "0.0", "0");
    ExpectFailure(spec, "undefined", InvalidType(kTypeInteger, kTypeUndefined));
    ExpectFailure(spec, "null", InvalidType(kTypeInteger, kTypeNull));
    ExpectFailure(spec, "1.1", InvalidType(kTypeInteger, kTypeDouble));
    ExpectFailure(spec, "'foo'", InvalidType(kTypeInteger, kTypeString));
    ExpectFailure(spec, "'1'", InvalidType(kTypeInteger, kTypeString));
    ExpectFailure(spec, "({})", InvalidType(kTypeInteger, kTypeObject));
    ExpectFailure(spec, "[1]", InvalidType(kTypeInteger, kTypeList));
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'number'}"));
    ExpectSuccess(spec, "1", "1.0");
    ExpectSuccess(spec, "-1", "-1.0");
    ExpectSuccess(spec, "0", "0.0");
    ExpectSuccess(spec, "1.1", "1.1");
    ExpectSuccess(spec, "1.", "1.0");
    ExpectSuccess(spec, ".1", "0.1");
    ExpectFailure(spec, "undefined", InvalidType(kTypeDouble, kTypeUndefined));
    ExpectFailure(spec, "null", InvalidType(kTypeDouble, kTypeNull));
    ExpectFailure(spec, "'foo'", InvalidType(kTypeDouble, kTypeString));
    ExpectFailure(spec, "'1.1'", InvalidType(kTypeDouble, kTypeString));
    ExpectFailure(spec, "({})", InvalidType(kTypeDouble, kTypeObject));
    ExpectFailure(spec, "[1.1]", InvalidType(kTypeDouble, kTypeList));
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer', 'minimum': 1}"));
    ExpectSuccess(spec, "2", "2");
    ExpectSuccess(spec, "1", "1");
    ExpectFailure(spec, "0", NumberTooSmall(1));
    ExpectFailure(spec, "-1", NumberTooSmall(1));
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer', 'maximum': 10}"));
    ExpectSuccess(spec, "10", "10");
    ExpectSuccess(spec, "1", "1");
    ExpectFailure(spec, "11", NumberTooLarge(10));
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'string'}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "''", "''");
    ExpectFailure(spec, "1", InvalidType(kTypeString, kTypeInteger));
    ExpectFailure(spec, "({})", InvalidType(kTypeString, kTypeObject));
    ExpectFailure(spec, "['foo']", InvalidType(kTypeString, kTypeList));
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'string', 'enum': ['foo', 'bar']}"));
    std::set<std::string> valid_enums = {"foo", "bar"};
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']", InvalidType(kTypeString, kTypeList));
    ExpectFailure(spec, "'fo'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "'foobar'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "'baz'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "''", InvalidEnumValue(valid_enums));
  }

  {
    ArgumentSpec spec(*ValueFromString(
        "{'type': 'string', 'enum': [{'name': 'foo'}, {'name': 'bar'}]}"));
    std::set<std::string> valid_enums = {"foo", "bar"};
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']", InvalidType(kTypeString, kTypeList));
    ExpectFailure(spec, "'fo'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "'foobar'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "'baz'", InvalidEnumValue(valid_enums));
    ExpectFailure(spec, "''", InvalidEnumValue(valid_enums));
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'boolean'}"));
    ExpectSuccess(spec, "true", "true");
    ExpectSuccess(spec, "false", "false");
    ExpectFailure(spec, "1", InvalidType(kTypeBoolean, kTypeInteger));
    ExpectFailure(spec, "'true'", InvalidType(kTypeBoolean, kTypeString));
    ExpectFailure(spec, "null", InvalidType(kTypeBoolean, kTypeNull));
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'array', 'items': {'type': 'string'}}"));
    ExpectSuccess(spec, "[]", "[]");
    ExpectSuccess(spec, "['foo']", "['foo']");
    ExpectSuccess(spec, "['foo', 'bar']", "['foo','bar']");
    ExpectSuccess(spec, "var x = new Array(); x[0] = 'foo'; x;", "['foo']");
    ExpectFailure(spec, "'foo'", InvalidType(kTypeList, kTypeString));
    ExpectFailure(spec, "[1, 2]",
                  IndexError(0u, InvalidType(kTypeString, kTypeInteger)));
    ExpectFailure(spec, "['foo', 1]",
                  IndexError(1u, InvalidType(kTypeString, kTypeInteger)));
    ExpectFailure(spec,
                  "var x = ['a', 'b', 'c'];"
                  "x[4] = 'd';"  // x[3] is undefined, violating the spec.
                  "x;",
                  IndexError(3u, InvalidType(kTypeString, kTypeUndefined)));
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
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
    ExpectSuccess(spec, "({prop1: 'foo', prop2: 2})",
                  "{'prop1':'foo','prop2':2}");
    ExpectSuccess(spec, "({prop1: 'foo'})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "({prop1: 'foo', prop2: null})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "x = {}; x.prop1 = 'foo'; x;", "{'prop1':'foo'}");
    ExpectFailure(
        spec, "({prop1: 'foo', prop2: 'bar'})",
        PropertyError("prop2", InvalidType(kTypeInteger, kTypeString)));
    ExpectFailure(spec, "({prop2: 2})", MissingRequiredProperty("prop1"));
    // Unknown properties are not allowed.
    ExpectFailure(spec, "({prop1: 'foo', prop2: 2, prop3: 'blah'})",
                  UnexpectedProperty("prop3"));
    // We only consider properties on the object itself, not its prototype
    // chain.
    ExpectFailure(spec,
                  "function X() {}\n"
                  "X.prototype = { prop1: 'foo' };\n"
                  "var x = new X();\n"
                  "x;",
                  MissingRequiredProperty("prop1"));
    ExpectFailure(spec,
                  "function X() {}\n"
                  "X.prototype = { prop1: 'foo' };\n"
                  "function Y() { this.__proto__ = X.prototype; }\n"
                  "var z = new Y();\n"
                  "z;",
                  MissingRequiredProperty("prop1"));
    // Self-referential fun. Currently we don't have to worry about these much
    // because the spec won't match at some point (and V8ValueConverter has
    // cycle detection and will fail).
    ExpectFailure(
        spec, "x = {}; x.prop1 = x; x;",
        PropertyError("prop1", InvalidType(kTypeString, kTypeObject)));
    ExpectThrow(
        spec,
        "({ get prop1() { throw new Error('Badness'); }});",
        "Uncaught Error: Badness");
    ExpectThrow(spec,
                "x = {prop1: 'foo'};\n"
                "Object.defineProperty(\n"
                "    x, 'prop2',\n"
                "    {\n"
                "      get: () => { throw new Error('Badness'); },\n"
                "      enumerable: true,\n"
                "});\n"
                "x;",
                "Uncaught Error: Badness");
    // By default, properties from Object.defineProperty() aren't enumerable,
    // so they will be ignored in our matching.
    ExpectSuccess(spec,
                  "x = {prop1: 'foo'};\n"
                  "Object.defineProperty(\n"
                  "    x, 'prop2',\n"
                  "    { get: () => { throw new Error('Badness'); } });\n"
                  "x;",
                  "{'prop1':'foo'}");
  }

  {
    const char kFunctionSpec[] = "{ 'type': 'function' }";
    ArgumentSpec spec(*ValueFromString(kFunctionSpec));
    // Functions are serialized as empty dictionaries.
    ExpectSuccess(spec, "(function() {})", "{}");
    ExpectSuccessWithNoConversion(spec, "(function() {})");
    ExpectSuccessWithNoConversion(spec, "(function(a, b) { a(); b(); })");
    ExpectSuccessWithNoConversion(spec, "(function(a, b) { a(); b(); })");
    ExpectFailureWithNoConversion(spec, "({a: function() {}})",
                                  InvalidType(kTypeFunction, kTypeObject));
    ExpectFailureWithNoConversion(spec, "([function() {}])",
                                  InvalidType(kTypeFunction, kTypeList));
    ExpectFailureWithNoConversion(spec, "1",
                                  InvalidType(kTypeFunction, kTypeInteger));
  }

  {
    const char kBinarySpec[] = "{ 'type': 'binary' }";
    ArgumentSpec spec(*ValueFromString(kBinarySpec));
    // Simple case: empty ArrayBuffer -> empty BinaryValue.
    ExpectSuccess(spec, "(new ArrayBuffer())",
                  base::Value(base::Value::Type::BINARY));
    {
      // A non-empty (but zero-filled) ArrayBufferView.
      const char kBuffer[] = {0, 0, 0, 0};
      std::unique_ptr<base::Value> expected_value =
          base::Value::CreateWithCopiedBuffer(kBuffer, arraysize(kBuffer));
      ASSERT_TRUE(expected_value);
      ExpectSuccessWithNoConversion(spec, "(new Int32Array(2))");
    }
    {
      // Actual data.
      const char kBuffer[] = {'p', 'i', 'n', 'g'};
      std::unique_ptr<base::Value> expected_value =
          base::Value::CreateWithCopiedBuffer(kBuffer, arraysize(kBuffer));
      ASSERT_TRUE(expected_value);
      ExpectSuccess(spec,
                    "var b = new ArrayBuffer(4);\n"
                    "var v = new Uint8Array(b);\n"
                    "var s = 'ping';\n"
                    "for (var i = 0; i < s.length; ++i)\n"
                    "  v[i] = s.charCodeAt(i);\n"
                    "b;",
                    *expected_value);
    }
    ExpectFailure(spec, "1", InvalidType(kTypeBinary, kTypeInteger));
  }
  {
    const char kAnySpec[] = "{ 'type': 'any' }";
    ArgumentSpec spec(*ValueFromString(kAnySpec));
    ExpectSuccess(spec, "42", "42");
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "({prop1:'bar'})", "{'prop1':'bar'}");
    ExpectSuccess(spec, "[1, 2, 3]", "[1,2,3]");
    ExpectSuccess(spec, "[1, 'a']", "[1,'a']");
    ExpectSuccess(spec, "null", base::Value());
    ExpectSuccess(spec, "({prop1: 'alpha', prop2: null})", "{'prop1':'alpha'}");
    ExpectSuccess(spec,
                  "x = {alpha: 'alpha'};\n"
                  "y = {beta: 'beta', x: x};\n"
                  "y;",
                  "{'beta':'beta','x':{'alpha':'alpha'}}");
    // We don't serialize undefined.
    // TODO(devlin): This matches current behavior, but should it? Part of the
    // problem is that base::Values don't differentiate between undefined and
    // null, which is a potentially important distinction. However, this means
    // that in serialization of an object {a: 1, foo:undefined}, we lose the
    // 'foo' property.
    ExpectFailure(spec, "undefined", UnserializableValue());

    ExpectSuccess(spec, "({prop1: 1, prop2: undefined})", "{'prop1':1}");
  }
}

TEST_F(ArgumentSpecUnitTest, TypeRefsTest) {
  using namespace api_errors;
  const char kObjectType[] =
      "{"
      "  'id': 'refObj',"
      "  'type': 'object',"
      "  'properties': {"
      "    'prop1': {'type': 'string'},"
      "    'prop2': {'type': 'integer', 'optional': true}"
      "  }"
      "}";
  const char kEnumType[] =
      "{'id': 'refEnum', 'type': 'string', 'enum': ['alpha', 'beta']}";
  AddTypeRef("refObj",
             base::MakeUnique<ArgumentSpec>(*ValueFromString(kObjectType)));
  AddTypeRef("refEnum",
             base::MakeUnique<ArgumentSpec>(*ValueFromString(kEnumType)));
  std::set<std::string> valid_enums = {"alpha", "beta"};

  {
    const char kObjectWithRefEnumSpec[] =
        "{"
        "  'name': 'objWithRefEnum',"
        "  'type': 'object',"
        "  'properties': {"
        "    'e': {'$ref': 'refEnum'},"
        "    'sub': {'type': 'integer'}"
        "  }"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectWithRefEnumSpec));
    ExpectSuccess(spec, "({e: 'alpha', sub: 1})", "{'e':'alpha','sub':1}");
    ExpectSuccess(spec, "({e: 'beta', sub: 1})", "{'e':'beta','sub':1}");
    ExpectFailure(spec, "({e: 'gamma', sub: 1})",
                  PropertyError("e", InvalidEnumValue(valid_enums)));
    ExpectFailure(spec, "({e: 'alpha'})", MissingRequiredProperty("sub"));
  }

  {
    const char kObjectWithRefObjectSpec[] =
        "{"
        "  'name': 'objWithRefObject',"
        "  'type': 'object',"
        "  'properties': {"
        "    'o': {'$ref': 'refObj'}"
        "  }"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectWithRefObjectSpec));
    ExpectSuccess(spec, "({o: {prop1: 'foo'}})", "{'o':{'prop1':'foo'}}");
    ExpectSuccess(spec, "({o: {prop1: 'foo', prop2: 2}})",
                  "{'o':{'prop1':'foo','prop2':2}}");
    ExpectFailure(
        spec, "({o: {prop1: 1}})",
        PropertyError("o", PropertyError("prop1", InvalidType(kTypeString,
                                                              kTypeInteger))));
  }

  {
    const char kRefEnumListSpec[] =
        "{'type': 'array', 'items': {'$ref': 'refEnum'}}";
    ArgumentSpec spec(*ValueFromString(kRefEnumListSpec));
    ExpectSuccess(spec, "['alpha']", "['alpha']");
    ExpectSuccess(spec, "['alpha', 'alpha']", "['alpha','alpha']");
    ExpectSuccess(spec, "['alpha', 'beta']", "['alpha','beta']");
    ExpectFailure(spec, "['alpha', 'beta', 'gamma']",
                  IndexError(2u, InvalidEnumValue(valid_enums)));
  }
}

TEST_F(ArgumentSpecUnitTest, TypeChoicesTest) {
  using namespace api_errors;
  {
    const char kSimpleChoices[] =
        "{'choices': [{'type': 'string'}, {'type': 'integer'}]}";
    ArgumentSpec spec(*ValueFromString(kSimpleChoices));
    ExpectSuccess(spec, "'alpha'", "'alpha'");
    ExpectSuccess(spec, "42", "42");
    ExpectFailure(spec, "true", InvalidChoice());
  }

  {
    const char kComplexChoices[] =
        "{"
        "  'choices': ["
        "    {'type': 'array', 'items': {'type': 'string'}},"
        "    {'type': 'object', 'properties': {'prop1': {'type': 'string'}}}"
        "  ]"
        "}";
    ArgumentSpec spec(*ValueFromString(kComplexChoices));
    ExpectSuccess(spec, "['alpha']", "['alpha']");
    ExpectSuccess(spec, "['alpha', 'beta']", "['alpha','beta']");
    ExpectSuccess(spec, "({prop1: 'alpha'})", "{'prop1':'alpha'}");
    ExpectFailure(spec, "({prop1: 1})", InvalidChoice());
    ExpectFailure(spec, "'alpha'", InvalidChoice());
    ExpectFailure(spec, "42", InvalidChoice());
  }
}

TEST_F(ArgumentSpecUnitTest, AdditionalPropertiesTest) {
  using namespace api_errors;
  {
    const char kOnlyAnyAdditionalProperties[] =
        "{"
        "  'type': 'object',"
        "  'additionalProperties': {'type': 'any'}"
        "}";
    ArgumentSpec spec(*ValueFromString(kOnlyAnyAdditionalProperties));
    ExpectSuccess(spec, "({prop1: 'alpha', prop2: 42, prop3: {foo: 'bar'}})",
                  "{'prop1':'alpha','prop2':42,'prop3':{'foo':'bar'}}");
    ExpectSuccess(spec, "({})", "{}");
    // Test some crazy keys.
    ExpectSuccess(spec,
                  "var x = {};\n"
                  "var y = {prop1: 'alpha'};\n"
                  "y[42] = 'beta';\n"
                  "y[x] = 'gamma';\n"
                  "y[undefined] = 'delta';\n"
                  "y;",
                  "{'42':'beta','[object Object]':'gamma','prop1':'alpha',"
                  "'undefined':'delta'}");
    // We (typically*, see "Fun case" below) don't serialize properties on an
    // object prototype.
    ExpectSuccess(spec,
                  "({\n"
                  "  __proto__: {protoProp: 'proto'},\n"
                  "  instanceProp: 'instance'\n"
                  "})",
                  "{'instanceProp':'instance'}");
    // Fun case: Remove a property as a result of getting another. Currently,
    // we don't check each property with HasOwnProperty() during iteration, so
    // Fun case: Remove a property as a result of getting another. Currently,
    // we don't check each property with HasOwnProperty() during iteration, so
    // we still try to serialize it. But we don't serialize undefined, so in the
    // case of the property not being defined on the prototype, this works as
    // expected.
    ExpectSuccess(spec,
                  "var x = {};\n"
                  "Object.defineProperty(\n"
                  "    x, 'alpha',\n"
                  "    {\n"
                  "      enumerable: true,\n"
                  "      get: () => { delete x.omega; return 'alpha'; }\n"
                  "    });\n"
                  "x.omega = 'omega';\n"
                  "x;",
                  "{'alpha':'alpha'}");
    // Fun case continued: If an object removes the property, and the property
    // *is* present on the prototype, then we serialize the value from the
    // prototype. This is inconsistent, but only manifests scripts are doing
    // crazy things (and is still safe).
    // TODO(devlin): We *could* add a HasOwnProperty() check, in which case
    // the result of this call should be {'alpha':'alpha'}.
    ExpectSuccess(spec,
                  "var x = {\n"
                  "  __proto__: { omega: 'different omega' }\n"
                  "};\n"
                  "Object.defineProperty(\n"
                  "    x, 'alpha',\n"
                  "    {\n"
                  "      enumerable: true,\n"
                  "      get: () => { delete x.omega; return 'alpha'; }\n"
                  "    });\n"
                  "x.omega = 'omega';\n"
                  "x;",
                  "{'alpha':'alpha','omega':'different omega'}");
  }
  {
    const char kPropertiesAndAnyAdditionalProperties[] =
        "{"
        "  'type': 'object',"
        "  'properties': {"
        "    'prop1': {'type': 'string'}"
        "  },"
        "  'additionalProperties': {'type': 'any'}"
        "}";
    ArgumentSpec spec(*ValueFromString(kPropertiesAndAnyAdditionalProperties));
    ExpectSuccess(spec, "({prop1: 'alpha', prop2: 42, prop3: {foo: 'bar'}})",
                  "{'prop1':'alpha','prop2':42,'prop3':{'foo':'bar'}}");
    // Additional properties are optional.
    ExpectSuccess(spec, "({prop1: 'foo'})", "{'prop1':'foo'}");
    ExpectFailure(spec, "({prop2: 42, prop3: {foo: 'bar'}})",
                  MissingRequiredProperty("prop1"));
    ExpectFailure(
        spec, "({prop1: 42})",
        PropertyError("prop1", InvalidType(kTypeString, kTypeInteger)));
  }
  {
    const char kTypedAdditionalProperties[] =
        "{"
        "  'type': 'object',"
        "  'additionalProperties': {'type': 'string'}"
        "}";
    ArgumentSpec spec(*ValueFromString(kTypedAdditionalProperties));
    ExpectSuccess(spec, "({prop1: 'alpha', prop2: 'beta', prop3: 'gamma'})",
                  "{'prop1':'alpha','prop2':'beta','prop3':'gamma'}");
    ExpectFailure(
        spec, "({prop1: 'alpha', prop2: 42})",
        PropertyError("prop2", InvalidType(kTypeString, kTypeInteger)));
  }
}

TEST_F(ArgumentSpecUnitTest, InstanceOfTest) {
  using namespace api_errors;
  {
    const char kInstanceOfRegExp[] =
        "{"
        "  'type': 'object',"
        "  'isInstanceOf': 'RegExp'"
        "}";
    ArgumentSpec spec(*ValueFromString(kInstanceOfRegExp));
    ExpectSuccess(spec, "(new RegExp())", "{}");
    ExpectSuccess(spec, "({ __proto__: RegExp.prototype })", "{}");
    ExpectSuccess(spec,
                  "(function() {\n"
                  "  function subRegExp() {}\n"
                  "  subRegExp.prototype = { __proto__: RegExp.prototype };\n"
                  "  return new subRegExp();\n"
                  "})()",
                  "{}");
    ExpectSuccess(spec,
                  "(function() {\n"
                  "  function RegExp() {}\n"
                  "  return new RegExp();\n"
                  "})()",
                  "{}");
    ExpectFailure(spec, "({})", NotAnInstance("RegExp"));
    ExpectFailure(spec, "('')", InvalidType("RegExp", kTypeString));
    ExpectFailure(spec, "('.*')", InvalidType("RegExp", kTypeString));
    ExpectFailure(spec, "({ __proto__: Date.prototype })",
                  NotAnInstance("RegExp"));
  }

  {
    const char kInstanceOfCustomClass[] =
        "{"
        "  'type': 'object',"
        "  'isInstanceOf': 'customClass'"
        "}";
    ArgumentSpec spec(*ValueFromString(kInstanceOfCustomClass));
    ExpectSuccess(spec,
                  "(function() {\n"
                  "  function customClass() {}\n"
                  "  return new customClass();\n"
                  "})()",
                  "{}");
    ExpectSuccess(spec,
                  "(function() {\n"
                  "  function customClass() {}\n"
                  "  function otherClass() {}\n"
                  "  otherClass.prototype = \n"
                  "      { __proto__: customClass.prototype };\n"
                  "  return new otherClass();\n"
                  "})()",
                  "{}");
    ExpectFailure(spec, "({})", NotAnInstance("customClass"));
    ExpectFailure(spec,
                  "(function() {\n"
                  "  function otherClass() {}\n"
                  "  return new otherClass();\n"
                  "})()",
                  NotAnInstance("customClass"));
  }
}

TEST_F(ArgumentSpecUnitTest, MinAndMaxLengths) {
  using namespace api_errors;
  {
    const char kMinLengthString[] = "{'type': 'string', 'minLength': 3}";
    ArgumentSpec spec(*ValueFromString(kMinLengthString));
    ExpectSuccess(spec, "'aaa'", "'aaa'");
    ExpectSuccess(spec, "'aaaa'", "'aaaa'");
    ExpectFailure(spec, "'aa'", TooFewStringChars(3, 2));
    ExpectFailure(spec, "''", TooFewStringChars(3, 0));
  }

  {
    const char kMaxLengthString[] = "{'type': 'string', 'maxLength': 3}";
    ArgumentSpec spec(*ValueFromString(kMaxLengthString));
    ExpectSuccess(spec, "'aaa'", "'aaa'");
    ExpectSuccess(spec, "'aa'", "'aa'");
    ExpectSuccess(spec, "''", "''");
    ExpectFailure(spec, "'aaaa'", TooManyStringChars(3, 4));
  }

  {
    const char kMinLengthArray[] =
        "{'type': 'array', 'items': {'type': 'integer'}, 'minItems': 3}";
    ArgumentSpec spec(*ValueFromString(kMinLengthArray));
    ExpectSuccess(spec, "[1, 2, 3]", "[1,2,3]");
    ExpectSuccess(spec, "[1, 2, 3, 4]", "[1,2,3,4]");
    ExpectFailure(spec, "[1, 2]", TooFewArrayItems(3, 2));
    ExpectFailure(spec, "[]", TooFewArrayItems(3, 0));
  }

  {
    const char kMaxLengthArray[] =
        "{'type': 'array', 'items': {'type': 'integer'}, 'maxItems': 3}";
    ArgumentSpec spec(*ValueFromString(kMaxLengthArray));
    ExpectSuccess(spec, "[1, 2, 3]", "[1,2,3]");
    ExpectSuccess(spec, "[1, 2]", "[1,2]");
    ExpectSuccess(spec, "[]", "[]");
    ExpectFailure(spec, "[1, 2, 3, 4]", TooManyArrayItems(3, 4));
  }
}

TEST_F(ArgumentSpecUnitTest, PreserveNull) {
  using namespace api_errors;
  {
    const char kObjectSpec[] =
        "{"
        "  'type': 'object',"
        "  'additionalProperties': {'type': 'any'},"
        "  'preserveNull': true"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
    ExpectSuccess(spec, "({foo: 1, bar: null})", "{'bar':null,'foo':1}");
    // Subproperties shouldn't preserve null (if not specified).
    ExpectSuccess(spec, "({prop: {subprop1: 'foo', subprop2: null}})",
                  "{'prop':{'subprop1':'foo'}}");
  }

  {
    const char kObjectSpec[] =
        "{"
        "  'type': 'object',"
        "  'additionalProperties': {'type': 'any', 'preserveNull': true},"
        "  'preserveNull': true"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
    ExpectSuccess(spec, "({foo: 1, bar: null})", "{'bar':null,'foo':1}");
    // Here, subproperties should preserve null.
    ExpectSuccess(spec, "({prop: {subprop1: 'foo', subprop2: null}})",
                  "{'prop':{'subprop1':'foo','subprop2':null}}");
  }

  {
    const char kObjectSpec[] =
        "{"
        "  'type': 'object',"
        "  'properties': {'prop1': {'type': 'string', 'optional': true}},"
        "  'preserveNull': true"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
    ExpectSuccess(spec, "({})", "{}");
    ExpectSuccess(spec, "({prop1: null})", "{'prop1':null}");
    ExpectSuccess(spec, "({prop1: 'foo'})", "{'prop1':'foo'}");
    // Undefined should not be preserved.
    ExpectSuccess(spec, "({prop1: undefined})", "{}");
    // preserveNull shouldn't affect normal parsing restrictions.
    ExpectFailure(
        spec, "({prop1: 1})",
        PropertyError("prop1", InvalidType(kTypeString, kTypeInteger)));
  }
}

TEST_F(ArgumentSpecUnitTest, NaNFun) {
  using namespace api_errors;

  {
    const char kAnySpec[] = "{'type': 'any'}";
    ArgumentSpec spec(*ValueFromString(kAnySpec));
    ExpectFailure(spec, "NaN", UnserializableValue());
  }

  {
    const char kObjectWithAnyPropertiesSpec[] =
        "{'type': 'object', 'additionalProperties': {'type': 'any'}}";
    ArgumentSpec spec(*ValueFromString(kObjectWithAnyPropertiesSpec));
    ExpectSuccess(spec, "({foo: NaN, bar: 'baz'})", "{'bar':'baz'}");
  }
}

TEST_F(ArgumentSpecUnitTest, GetTypeName) {
  struct {
    ArgumentType type;
    const char* expected_type_name;
  } simple_cases[] = {
      {ArgumentType::BOOLEAN, api_errors::kTypeBoolean},
      {ArgumentType::INTEGER, api_errors::kTypeInteger},
      {ArgumentType::OBJECT, api_errors::kTypeObject},
      {ArgumentType::LIST, api_errors::kTypeList},
      {ArgumentType::BINARY, api_errors::kTypeBinary},
      {ArgumentType::FUNCTION, api_errors::kTypeFunction},
      {ArgumentType::ANY, api_errors::kTypeAny},
  };

  for (const auto& test_case : simple_cases) {
    ArgumentSpec spec(test_case.type);
    EXPECT_EQ(test_case.expected_type_name, spec.GetTypeName());
  }

  {
    const char kRefName[] = "someRef";
    ArgumentSpec ref_spec(ArgumentType::REF);
    ref_spec.set_ref(kRefName);
    EXPECT_EQ(kRefName, ref_spec.GetTypeName());
  }

  {
    std::vector<std::unique_ptr<ArgumentSpec>> choices;
    choices.push_back(base::MakeUnique<ArgumentSpec>(ArgumentType::INTEGER));
    choices.push_back(base::MakeUnique<ArgumentSpec>(ArgumentType::STRING));
    ArgumentSpec choices_spec(ArgumentType::CHOICES);
    choices_spec.set_choices(std::move(choices));
    EXPECT_EQ("[integer|string]", choices_spec.GetTypeName());
  }
}

}  // namespace extensions
