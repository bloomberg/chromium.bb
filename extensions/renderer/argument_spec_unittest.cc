// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "extensions/renderer/argument_spec.h"
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
  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const std::string& expected_json_single_quotes) {
    RunTest(spec, script_source, true, TestResult::PASS,
            ReplaceSingleQuotes(expected_json_single_quotes), nullptr,
            std::string());
  }

  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const base::Value& expected_value) {
    RunTest(spec, script_source, true, TestResult::PASS, std::string(),
            &expected_value, std::string());
  }

  void ExpectSuccessWithNoConversion(const ArgumentSpec& spec,
                                     const std::string& script_source) {
    RunTest(spec, script_source, false, TestResult::PASS, std::string(),
            nullptr, std::string());
  }

  void ExpectFailure(const ArgumentSpec& spec,
                     const std::string& script_source) {
    RunTest(spec, script_source, true, TestResult::FAIL, std::string(), nullptr,
            std::string());
  }

  void ExpectFailureWithNoConversion(const ArgumentSpec& spec,
                                     const std::string& script_source) {
    RunTest(spec, script_source, false, TestResult::FAIL, std::string(),
            nullptr, std::string());
  }

  void ExpectThrow(const ArgumentSpec& spec,
                   const std::string& script_source,
                   const std::string& expected_thrown_message) {
    RunTest(spec, script_source, true, TestResult::THROW, std::string(),
            nullptr, expected_thrown_message);
  }

  void AddTypeRef(const std::string& id, std::unique_ptr<ArgumentSpec> spec) {
    type_refs_.AddSpec(id, std::move(spec));
  }

 private:
  enum class TestResult { PASS, FAIL, THROW, };

  void RunTest(const ArgumentSpec& spec,
               const std::string& script_source,
               bool should_convert,
               TestResult expected_result,
               const std::string& expected_json,
               const base::Value* expected_value,
               const std::string& expected_thrown_message);

  APITypeReferenceMap type_refs_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpecUnitTest);
};

void ArgumentSpecUnitTest::RunTest(const ArgumentSpec& spec,
                                   const std::string& script_source,
                                   bool should_convert,
                                   TestResult expected_result,
                                   const std::string& expected_json,
                                   const base::Value* expected_value,
                                   const std::string& expected_thrown_message) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(instance_->isolate());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(instance_->isolate(), context_);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> val = V8ValueFromScriptSource(context, script_source);
  ASSERT_FALSE(val.IsEmpty()) << script_source;

  std::string error;
  std::unique_ptr<base::Value> out_value;
  bool did_succeed =
      spec.ParseArgument(context, val, type_refs_,
                         should_convert ? &out_value : nullptr, &error);
  bool should_succeed = expected_result == TestResult::PASS;
  ASSERT_EQ(should_succeed, did_succeed) << script_source << ", " << error;
  ASSERT_EQ(did_succeed && should_convert, !!out_value);
  bool should_throw = expected_result == TestResult::THROW;
  ASSERT_EQ(should_throw, try_catch.HasCaught()) << script_source;
  if (should_succeed && should_convert) {
    ASSERT_TRUE(out_value);
    if (expected_value)
      EXPECT_TRUE(expected_value->Equals(out_value.get())) << script_source;
    else
      EXPECT_EQ(expected_json, ValueToString(*out_value));
  } else if (should_throw) {
    EXPECT_EQ(expected_thrown_message,
              gin::V8ToString(try_catch.Message()->Get()));
  }
}

TEST_F(ArgumentSpecUnitTest, Test) {
  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer'}"));
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
    ArgumentSpec spec(*ValueFromString("{'type': 'integer', 'minimum': 1}"));
    ExpectSuccess(spec, "2", "2");
    ExpectSuccess(spec, "1", "1");
    ExpectFailure(spec, "0");
    ExpectFailure(spec, "-1");
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'string'}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "''", "''");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "{}");
    ExpectFailure(spec, "['foo']");
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'string', 'enum': ['foo', 'bar']}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']");
    ExpectFailure(spec, "'fo'");
    ExpectFailure(spec, "'foobar'");
    ExpectFailure(spec, "'baz'");
    ExpectFailure(spec, "''");
  }

  {
    ArgumentSpec spec(*ValueFromString(
        "{'type': 'string', 'enum': [{'name': 'foo'}, {'name': 'bar'}]}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']");
    ExpectFailure(spec, "'fo'");
    ExpectFailure(spec, "'foobar'");
    ExpectFailure(spec, "'baz'");
    ExpectFailure(spec, "''");
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'boolean'}"));
    ExpectSuccess(spec, "true", "true");
    ExpectSuccess(spec, "false", "false");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "'true'");
    ExpectFailure(spec, "null");
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'array', 'items': {'type': 'string'}}"));
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
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
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

  {
    const char kFunctionSpec[] = "{ 'type': 'function' }";
    ArgumentSpec spec(*ValueFromString(kFunctionSpec));
    ExpectSuccessWithNoConversion(spec, "(function() {})");
    ExpectSuccessWithNoConversion(spec, "(function(a, b) { a(); b(); })");
    ExpectSuccessWithNoConversion(spec, "(function(a, b) { a(); b(); })");
    ExpectFailureWithNoConversion(spec, "({a: function() {}})");
    ExpectFailureWithNoConversion(spec, "([function() {}])");
    ExpectFailureWithNoConversion(spec, "1");
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
      std::unique_ptr<base::BinaryValue> expected_value =
          base::BinaryValue::CreateWithCopiedBuffer(kBuffer,
                                                    arraysize(kBuffer));
      ASSERT_TRUE(expected_value);
      ExpectSuccessWithNoConversion(spec, "(new Int32Array(2))");
    }
    {
      // Actual data.
      const char kBuffer[] = {'p', 'i', 'n', 'g'};
      std::unique_ptr<base::BinaryValue> expected_value =
          base::BinaryValue::CreateWithCopiedBuffer(kBuffer,
                                                    arraysize(kBuffer));
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
    ExpectFailure(spec, "1");
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
    ExpectSuccess(spec, "({prop1: 'alpha', prop2: null})",
                  "{'prop1':'alpha','prop2':null}");
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
    ExpectFailure(spec, "undefined");
    ExpectSuccess(spec, "({prop1: 1, prop2: undefined})", "{'prop1':1}");
  }
}

TEST_F(ArgumentSpecUnitTest, TypeRefsTest) {
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
    ExpectFailure(spec, "({e: 'gamma', sub: 1})");
    ExpectFailure(spec, "({e: 'alpha'})");
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
    ExpectFailure(spec, "({o: {prop1: 1}})");
  }

  {
    const char kRefEnumListSpec[] =
        "{'type': 'array', 'items': {'$ref': 'refEnum'}}";
    ArgumentSpec spec(*ValueFromString(kRefEnumListSpec));
    ExpectSuccess(spec, "['alpha']", "['alpha']");
    ExpectSuccess(spec, "['alpha', 'alpha']", "['alpha','alpha']");
    ExpectSuccess(spec, "['alpha', 'beta']", "['alpha','beta']");
    ExpectFailure(spec, "['alpha', 'beta', 'gamma']");
  }
}

TEST_F(ArgumentSpecUnitTest, TypeChoicesTest) {
  {
    const char kSimpleChoices[] =
        "{'choices': [{'type': 'string'}, {'type': 'integer'}]}";
    ArgumentSpec spec(*ValueFromString(kSimpleChoices));
    ExpectSuccess(spec, "'alpha'", "'alpha'");
    ExpectSuccess(spec, "42", "42");
    ExpectFailure(spec, "true");
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
    ExpectFailure(spec, "({prop1: 1})");
    ExpectFailure(spec, "'alpha'");
    ExpectFailure(spec, "42");
  }
}

TEST_F(ArgumentSpecUnitTest, AdditionalPropertiesTest) {
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
    ExpectFailure(spec, "({prop2: 42, prop3: {foo: 'bar'}})");
    ExpectFailure(spec, "({prop1: 42})");
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
    ExpectFailure(spec, "({prop1: 'alpha', prop2: 42})");
  }
}

}  // namespace extensions
