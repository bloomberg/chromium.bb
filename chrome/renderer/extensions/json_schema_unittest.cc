// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/v8_unit_test.h"
#include "grit/renderer_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

static const char kJsonSchema[] = "json_schema.js";
static const char kJsonSchemaTest[] = "json_schema_test.js";

class JsonSchemaTest : public V8UnitTest {
 public:
  JsonSchemaTest() {}

  virtual void SetUp() {
    V8UnitTest::SetUp();

    // Add the json schema code to the context.
    std::string code = ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_JSON_SCHEMA_JS).as_string();

    // This is a nasty hack, but it is easier to test the code if we don't use
    // it as a v8 extension. So replace the only bit that relies on that with a
    // more easily testable implementation.
    ReplaceFirstSubstringAfterOffset(&code, 0,
        "native function GetChromeHidden();",
        "function GetChromeHidden() {\n"
        "  if (!this.chromeHidden) this.chromeHidden = {};\n"
        "  return this.chromeHidden;\n"
        "}");
    ExecuteScriptInContext(code, kJsonSchema);

    // Add the test functions to the context.
    FilePath test_js_file_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_js_file_path));
    test_js_file_path = test_js_file_path.AppendASCII("extensions");
    test_js_file_path = test_js_file_path.AppendASCII(kJsonSchemaTest);
    std::string test_js;
    ASSERT_TRUE(file_util::ReadFileToString(test_js_file_path, &test_js));
    ExecuteScriptInContext(test_js, kJsonSchemaTest);
  }
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

TEST_F(JsonSchemaTest, TestType) {
  TestFunction("testType");
}

TEST_F(JsonSchemaTest, TestTypeReference) {
  TestFunction("testTypeReference");
}
