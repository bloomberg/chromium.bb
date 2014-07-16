// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "extensions/renderer/module_system_test.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {
namespace {

class UtilsUnittest : public ModuleSystemTest {
 protected:
  void RegisterTestModule(const char* code) {
    env()->RegisterModule("test",
                          base::StringPrintf(
                              "var assert = requireNative('assert');\n"
                              "var AssertTrue = assert.AssertTrue;\n"
                              "var AssertFalse = assert.AssertFalse;\n"
                              "var utils = require('utils');\n"
                              "%s",
                              code));
  }

 private:
  virtual void SetUp() OVERRIDE {
    ModuleSystemTest::SetUp();

    env()->RegisterModule("utils", IDR_UTILS_JS);
    env()->OverrideNativeHandler("schema_registry",
                                 "exports.GetSchema = function() {};");
    env()->OverrideNativeHandler("logging",
                                 "exports.CHECK = function() {};\n"
                                 "exports.WARNING = function() {};");
  }
};

TEST_F(UtilsUnittest, TestNothing) {
  ExpectNoAssertionsMade();
}

TEST_F(UtilsUnittest, SuperClass) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  RegisterTestModule(
      "function SuperClassImpl() {}\n"
      "\n"
      "SuperClassImpl.prototype = {\n"
      "  attrA: 'aSuper',\n"
      "  attrB: 'bSuper',\n"
      "  func: function() { return 'func'; },\n"
      "  superFunc: function() { return 'superFunc'; }\n"
      "};\n"
      "\n"
      "function SubClassImpl() {\n"
      "  SuperClassImpl.call(this);\n"
      "}\n"
      "\n"
      "SubClassImpl.prototype = {\n"
      "  __proto__: SuperClassImpl.prototype,\n"
      "  attrA: 'aSub',\n"
      "  attrC: 'cSub',\n"
      "  func: function() { return 'overridden'; },\n"
      "  subFunc: function() { return 'subFunc'; }\n"
      "};\n"
      "\n"
      "var SuperClass = utils.expose('SuperClass',\n"
      "                              SuperClassImpl,\n"
      "                              { functions: ['func', 'superFunc'],\n"
      "                                properties: ['attrA', 'attrB'] });\n"
      "\n"
      "var SubClass = utils.expose('SubClass',\n"
      "                            SubClassImpl,\n"
      "                            { superclass: SuperClass,\n"
      "                              functions: ['subFunc'],\n"
      "                              properties: ['attrC'] });\n"
      "\n"
      "var supe = new SuperClass();\n"
      "AssertTrue(supe.attrA == 'aSuper');\n"
      "AssertTrue(supe.attrB == 'bSuper');\n"
      "AssertFalse('attrC' in supe);\n"
      "AssertTrue(supe.func() == 'func');\n"
      "AssertTrue('superFunc' in supe);\n"
      "AssertTrue(supe.superFunc() == 'superFunc');\n"
      "AssertFalse('subFunc' in supe);\n"
      "AssertTrue(supe instanceof SuperClass);\n"
      "\n"
      "var sub = new SubClass();\n"
      "AssertTrue(sub.attrA == 'aSub');\n"
      "AssertTrue(sub.attrB == 'bSuper');\n"
      "AssertTrue(sub.attrC == 'cSub');\n"
      "AssertTrue(sub.func() == 'overridden');\n"
      "AssertTrue(sub.superFunc() == 'superFunc');\n"
      "AssertTrue('subFunc' in sub);\n"
      "AssertTrue(sub.subFunc() == 'subFunc');\n"
      "AssertTrue(sub instanceof SuperClass);\n"
      "AssertTrue(sub instanceof SubClass);\n"
      "\n"
      "function SubSubClassImpl() {}\n"
      "SubSubClassImpl.prototype = Object.create(SubClassImpl.prototype);\n"
      "SubSubClassImpl.prototype.subSubFunc = function() { return 'subsub'; }\n"
      "\n"
      "var SubSubClass = utils.expose('SubSubClass',\n"
      "                               SubSubClassImpl,\n"
      "                               { superclass: SubClass,\n"
      "                                 functions: ['subSubFunc'] });\n"
      "var subsub = new SubSubClass();\n"
      "AssertTrue(subsub.attrA == 'aSub');\n"
      "AssertTrue(subsub.attrB == 'bSuper');\n"
      "AssertTrue(subsub.attrC == 'cSub');\n"
      "AssertTrue(subsub.func() == 'overridden');\n"
      "AssertTrue(subsub.superFunc() == 'superFunc');\n"
      "AssertTrue(subsub.subFunc() == 'subFunc');\n"
      "AssertTrue(subsub.subSubFunc() == 'subsub');\n"
      "AssertTrue(subsub instanceof SuperClass);\n"
      "AssertTrue(subsub instanceof SubClass);\n"
      "AssertTrue(subsub instanceof SubSubClass);\n");

  env()->module_system()->Require("test");
}

}  // namespace
}  // namespace extensions
