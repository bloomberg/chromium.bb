// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_signature.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_invocation_errors.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "extensions/renderer/argument_spec.h"
#include "extensions/renderer/argument_spec_builder.h"
#include "gin/converter.h"

namespace extensions {
namespace {

using SpecVector = std::vector<std::unique_ptr<ArgumentSpec>>;

std::unique_ptr<APISignature> OneString() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::STRING, "string").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> StringAndInt() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::STRING, "string").Build());
  specs.push_back(ArgumentSpecBuilder(ArgumentType::INTEGER, "int").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> StringOptionalIntAndBool() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::STRING, "string").Build());
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::INTEGER, "int").MakeOptional().Build());
  specs.push_back(ArgumentSpecBuilder(ArgumentType::BOOLEAN, "bool").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> OneObject() {
  SpecVector specs;
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::OBJECT, "obj")
          .AddProperty("prop1",
                       ArgumentSpecBuilder(ArgumentType::STRING).Build())
          .AddProperty(
              "prop2",
              ArgumentSpecBuilder(ArgumentType::STRING).MakeOptional().Build())
          .Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> NoArgs() {
  return base::MakeUnique<APISignature>(SpecVector());
}

std::unique_ptr<APISignature> IntAndCallback() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::INTEGER, "int").Build());
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::FUNCTION, "callback").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> OptionalIntAndCallback() {
  SpecVector specs;
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::INTEGER, "int").MakeOptional().Build());
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::FUNCTION, "callback").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> OptionalCallback() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::FUNCTION, "callback")
                      .MakeOptional()
                      .Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> IntAnyOptionalObjectOptionalCallback() {
  SpecVector specs;
  specs.push_back(ArgumentSpecBuilder(ArgumentType::INTEGER, "int").Build());
  specs.push_back(ArgumentSpecBuilder(ArgumentType::ANY, "any").Build());
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::OBJECT, "obj")
          .AddProperty(
              "prop",
              ArgumentSpecBuilder(ArgumentType::INTEGER).MakeOptional().Build())
          .MakeOptional()
          .Build());
  specs.push_back(ArgumentSpecBuilder(ArgumentType::FUNCTION, "callback")
                      .MakeOptional()
                      .Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> RefObj() {
  SpecVector specs;
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::REF, "obj").SetRef("refObj").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

std::unique_ptr<APISignature> RefEnum() {
  SpecVector specs;
  specs.push_back(
      ArgumentSpecBuilder(ArgumentType::REF, "enum").SetRef("refEnum").Build());
  return base::MakeUnique<APISignature>(std::move(specs));
}

}  // namespace

class APISignatureTest : public APIBindingTest {
 public:
  APISignatureTest()
      : type_refs_(APITypeReferenceMap::InitializeTypeCallback()) {}
  ~APISignatureTest() override = default;

  void SetUp() override {
    APIBindingTest::SetUp();

    std::unique_ptr<ArgumentSpec> ref_obj_spec =
        ArgumentSpecBuilder(ArgumentType::OBJECT)
            .AddProperty("prop1",
                         ArgumentSpecBuilder(ArgumentType::STRING).Build())
            .AddProperty("prop2", ArgumentSpecBuilder(ArgumentType::INTEGER)
                                      .MakeOptional()
                                      .Build())
            .Build();
    type_refs_.AddSpec("refObj", std::move(ref_obj_spec));

    type_refs_.AddSpec("refEnum", ArgumentSpecBuilder(ArgumentType::STRING)
                                      .SetEnums({"alpha", "beta"})
                                      .Build());
  }

  void ExpectPass(const APISignature& signature,
                  base::StringPiece arg_values,
                  base::StringPiece expected_parsed_args,
                  bool expect_callback) {
    RunTest(signature, arg_values, expected_parsed_args, expect_callback, true,
            std::string());
  }

  void ExpectFailure(const APISignature& signature,
                     base::StringPiece arg_values,
                     const std::string& expected_error) {
    RunTest(signature, arg_values, base::StringPiece(), false, false,
            expected_error);
  }

 private:
  void RunTest(const APISignature& signature,
               base::StringPiece arg_values,
               base::StringPiece expected_parsed_args,
               bool expect_callback,
               bool should_succeed,
               const std::string& expected_error) {
    SCOPED_TRACE(arg_values);
    v8::Local<v8::Context> context = MainContext();
    v8::Local<v8::Value> v8_args = V8ValueFromScriptSource(context, arg_values);
    ASSERT_FALSE(v8_args.IsEmpty());
    ASSERT_TRUE(v8_args->IsArray());
    std::vector<v8::Local<v8::Value>> vector_args;
    ASSERT_TRUE(gin::ConvertFromV8(isolate(), v8_args, &vector_args));

    std::unique_ptr<base::ListValue> result;
    v8::Local<v8::Function> callback;
    std::string error;
    bool success = signature.ParseArgumentsToJSON(
        context, vector_args, type_refs_, &result, &callback, &error);
    EXPECT_EQ(should_succeed, success);
    ASSERT_EQ(should_succeed, !!result);
    EXPECT_EQ(expect_callback, !callback.IsEmpty());
    if (should_succeed) {
      EXPECT_EQ(ReplaceSingleQuotes(expected_parsed_args),
                ValueToString(*result));
    } else {
      EXPECT_EQ(expected_error, error);
    }
  }

  APITypeReferenceMap type_refs_;

  DISALLOW_COPY_AND_ASSIGN(APISignatureTest);
};

TEST_F(APISignatureTest, BasicSignatureParsing) {
  using namespace api_errors;

  v8::HandleScope handle_scope(isolate());

  {
    auto signature = OneString();
    ExpectPass(*signature, "['foo']", "['foo']", false);
    ExpectPass(*signature, "['']", "['']", false);
    ExpectFailure(
        *signature, "[1]",
        ArgumentError("string", InvalidType(kTypeString, kTypeInteger)));
    ExpectFailure(*signature, "[]", MissingRequiredArgument("string"));
    ExpectFailure(
        *signature, "[{}]",
        ArgumentError("string", InvalidType(kTypeString, kTypeObject)));
    ExpectFailure(*signature, "['foo', 'bar']", TooManyArguments());
  }

  {
    auto signature = StringAndInt();
    ExpectPass(*signature, "['foo', 42]", "['foo',42]", false);
    ExpectPass(*signature, "['foo', -1]", "['foo',-1]", false);
    ExpectFailure(
        *signature, "[1]",
        ArgumentError("string", InvalidType(kTypeString, kTypeInteger)));
    ExpectFailure(*signature, "['foo'];", MissingRequiredArgument("int"));
    ExpectFailure(
        *signature, "[1, 'foo']",
        ArgumentError("string", InvalidType(kTypeString, kTypeInteger)));
    ExpectFailure(*signature, "['foo', 'foo']",
                  ArgumentError("int", InvalidType(kTypeInteger, kTypeString)));
    ExpectFailure(*signature, "['foo', '1']",
                  ArgumentError("int", InvalidType(kTypeInteger, kTypeString)));
    ExpectFailure(*signature, "['foo', 2.3]",
                  ArgumentError("int", InvalidType(kTypeInteger, kTypeDouble)));
  }

  {
    auto signature = StringOptionalIntAndBool();
    ExpectPass(*signature, "['foo', 42, true]", "['foo',42,true]", false);
    ExpectPass(*signature, "['foo', true]", "['foo',null,true]", false);
    ExpectFailure(
        *signature, "['foo', 'bar', true]",
        ArgumentError("bool", InvalidType(kTypeBoolean, kTypeString)));
  }

  {
    auto signature = OneObject();
    ExpectPass(*signature, "[{prop1: 'foo'}]", "[{'prop1':'foo'}]", false);
    ExpectFailure(*signature,
                  "[{ get prop1() { throw new Error('Badness'); } }]",
                  ArgumentError("obj", ScriptThrewError()));
  }

  {
    auto signature = NoArgs();
    ExpectPass(*signature, "[]", "[]", false);
    ExpectFailure(*signature, "[0]", TooManyArguments());
    ExpectFailure(*signature, "['']", TooManyArguments());
    ExpectFailure(*signature, "[null]", TooManyArguments());
    ExpectFailure(*signature, "[undefined]", TooManyArguments());
  }

  {
    auto signature = IntAndCallback();
    ExpectPass(*signature, "[1, function() {}]", "[1]", true);
    ExpectFailure(
        *signature, "[function() {}]",
        ArgumentError("int", InvalidType(kTypeInteger, kTypeFunction)));
    ExpectFailure(*signature, "[1]", MissingRequiredArgument("callback"));
  }

  {
    auto signature = OptionalIntAndCallback();
    ExpectPass(*signature, "[1, function() {}]", "[1]", true);
    ExpectPass(*signature, "[function() {}]", "[null]", true);
    ExpectFailure(*signature, "[1]", MissingRequiredArgument("callback"));
  }

  {
    auto signature = OptionalCallback();
    ExpectPass(*signature, "[function() {}]", "[]", true);
    ExpectPass(*signature, "[]", "[]", false);
    ExpectPass(*signature, "[undefined]", "[]", false);
    ExpectFailure(
        *signature, "[0]",
        ArgumentError("callback", InvalidType(kTypeFunction, kTypeInteger)));
  }

  {
    auto signature = IntAnyOptionalObjectOptionalCallback();
    ExpectPass(*signature, "[4, {foo: 'bar'}, function() {}]",
               "[4,{'foo':'bar'},null]", true);
    ExpectPass(*signature, "[4, {foo: 'bar'}]", "[4,{'foo':'bar'},null]",
               false);
    ExpectPass(*signature, "[4, {foo: 'bar'}, {}]", "[4,{'foo':'bar'},{}]",
               false);
    ExpectFailure(*signature, "[4, function() {}]",
                  ArgumentError("any", UnserializableValue()));
    ExpectFailure(*signature, "[4]", MissingRequiredArgument("any"));
  }
}

TEST_F(APISignatureTest, TypeRefsTest) {
  using namespace api_errors;

  v8::HandleScope handle_scope(isolate());

  {
    auto signature = RefObj();
    ExpectPass(*signature, "[{prop1: 'foo'}]", "[{'prop1':'foo'}]", false);
    ExpectPass(*signature, "[{prop1: 'foo', prop2: 2}]",
               "[{'prop1':'foo','prop2':2}]", false);
    ExpectFailure(
        *signature, "[{prop1: 'foo', prop2: 'a'}]",
        ArgumentError("obj", PropertyError("prop2", InvalidType(kTypeInteger,
                                                                kTypeString))));
  }

  {
    auto signature = RefEnum();
    ExpectPass(*signature, "['alpha']", "['alpha']", false);
    ExpectPass(*signature, "['beta']", "['beta']", false);
    ExpectFailure(*signature, "['gamma']",
                  ArgumentError("enum", InvalidEnumValue({"alpha", "beta"})));
  }
}

TEST_F(APISignatureTest, ExpectedSignature) {
  EXPECT_EQ("string string", OneString()->GetExpectedSignature());
  EXPECT_EQ("string string, integer int",
            StringAndInt()->GetExpectedSignature());
  EXPECT_EQ("string string, optional integer int, boolean bool",
            StringOptionalIntAndBool()->GetExpectedSignature());
  EXPECT_EQ("object obj", OneObject()->GetExpectedSignature());
  EXPECT_EQ("", NoArgs()->GetExpectedSignature());
  EXPECT_EQ("integer int, function callback",
            IntAndCallback()->GetExpectedSignature());
  EXPECT_EQ("optional integer int, function callback",
            OptionalIntAndCallback()->GetExpectedSignature());
  EXPECT_EQ("optional function callback",
            OptionalCallback()->GetExpectedSignature());
  EXPECT_EQ(
      "integer int, any any, optional object obj, optional function callback",
      IntAnyOptionalObjectOptionalCallback()->GetExpectedSignature());
  EXPECT_EQ("refObj obj", RefObj()->GetExpectedSignature());
  EXPECT_EQ("refEnum enum", RefEnum()->GetExpectedSignature());
}

}  // namespace extensions
