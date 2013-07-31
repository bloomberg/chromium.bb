// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/v8_schema_registry.h"
#include "chrome/test/base/module_system_test.h"
#include "grit/renderer_resources.h"

namespace extensions {

class JsonSchemaTest : public ModuleSystemTest {
 public:
  virtual void SetUp() OVERRIDE {
    ModuleSystemTest::SetUp();

    RegisterModule("json_schema", IDR_JSON_SCHEMA_JS);
    RegisterModule("utils", IDR_UTILS_JS);

    module_system_->RegisterNativeHandler("schema_registry",
        schema_registry_.AsNativeHandler());

    RegisterTestFile("json_schema_test", "json_schema_test.js");
  }

 protected:
  void TestFunction(const std::string& test_name) {
    module_system_->CallModuleMethod("json_schema_test", test_name);
  }

 private:
  V8SchemaRegistry schema_registry_;
};

TEST_F(JsonSchemaTest, TestFormatError) {
  TestFunction("testFormatError");
}

TEST_F(JsonSchemaTest, TestComplex) {
  TestFunction("testComplex");
}

TEST_F(JsonSchemaTest, TestEnum) {
  TestFunction("testEnum");
}

TEST_F(JsonSchemaTest, TestExtends) {
  TestFunction("testExtends");
}

TEST_F(JsonSchemaTest, TestObject) {
  TestFunction("testObject");
}

TEST_F(JsonSchemaTest, TestArrayTuple) {
  TestFunction("testArrayTuple");
}

TEST_F(JsonSchemaTest, TestArrayNonTuple) {
  TestFunction("testArrayNonTuple");
}

TEST_F(JsonSchemaTest, TestString) {
  TestFunction("testString");
}

TEST_F(JsonSchemaTest, TestNumber) {
  TestFunction("testNumber");
}

TEST_F(JsonSchemaTest, TestIntegerBounds) {
  TestFunction("testIntegerBounds");
}

TEST_F(JsonSchemaTest, TestType) {
  TestFunction("testType");
}

TEST_F(JsonSchemaTest, TestTypeReference) {
  TestFunction("testTypeReference");
}

TEST_F(JsonSchemaTest, TestGetAllTypesForSchema) {
  TestFunction("testGetAllTypesForSchema");
}

TEST_F(JsonSchemaTest, TestIsValidSchemaType) {
  TestFunction("testIsValidSchemaType");
}

TEST_F(JsonSchemaTest, TestCheckSchemaOverlap) {
  TestFunction("testCheckSchemaOverlap");
}

TEST_F(JsonSchemaTest, TestInstanceOf) {
  TestFunction("testInstanceOf");
}

}  // namespace extensions
