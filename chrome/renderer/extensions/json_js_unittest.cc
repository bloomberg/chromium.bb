// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/module_system_test.h"
#include "grit/renderer_resources.h"

namespace extensions {
namespace {

// Tests chrome/renderer/resources/json.js.
class JsonJsTest : public ModuleSystemTest {
  virtual void SetUp() OVERRIDE {
    ModuleSystemTest::SetUp();
    RegisterModule("json", IDR_JSON_JS);
  }

 protected:
  std::string requires() {
    return
      "var assert = requireNative('assert');\n"
      "var json = require('json');\n";
  }

  std::string test_body() {
    return
      "var str = '{\"a\":1,\"b\":true,\"c\":[\"hi\"]}';\n"
      "var obj = json.parse(str);\n"
      "assert.AssertTrue(json.stringify(obj) === str);\n"
      "assert.AssertTrue(obj.a === 1);\n"
      "assert.AssertTrue(obj.b === true);\n"
      "assert.AssertTrue(obj.c.length === 1);\n"
      "assert.AssertTrue(obj.c[0] === 'hi');\n";
  }
};

TEST_F(JsonJsTest, NoOverrides) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test", requires() + test_body());
  module_system_->Require("test");
}

TEST_F(JsonJsTest, EverythingOverridden) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  std::string test =
    requires() +
    "function fakeToJSON() { throw '42'; }\n"
    "Object.prototype.toJSON = fakeToJSON;\n"
    "Array.prototype.toJSON = fakeToJSON;\n"
    "Number.prototype.toJSON = fakeToJSON;\n"
    "Boolean.prototype.toJSON = fakeToJSON;\n"
    "String.prototype.toJSON = fakeToJSON;\n" +
    test_body() +
    "assert.AssertTrue(Object.prototype.toJSON == fakeToJSON);\n"
    "assert.AssertTrue(Array.prototype.toJSON == fakeToJSON);\n"
    "assert.AssertTrue(Number.prototype.toJSON == fakeToJSON);\n"
    "assert.AssertTrue(Boolean.prototype.toJSON == fakeToJSON);\n"
    "assert.AssertTrue(String.prototype.toJSON == fakeToJSON);\n";
  RegisterModule("test", test);
  module_system_->Require("test");
}

}  // namespace
}  // namespace extensions
