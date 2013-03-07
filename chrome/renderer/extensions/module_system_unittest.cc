// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/module_system_test.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/extensions/module_system.h"

using extensions::ModuleSystem;
using extensions::NativeHandler;

class CounterNatives : public NativeHandler {
 public:
  explicit CounterNatives(v8::Isolate* isolate)
      : NativeHandler(isolate), counter_(0) {
    RouteFunction("Get", base::Bind(&CounterNatives::Get,
        base::Unretained(this)));
    RouteFunction("Increment", base::Bind(&CounterNatives::Increment,
        base::Unretained(this)));
  }

  v8::Handle<v8::Value> Get(const v8::Arguments& args) {
    return v8::Integer::New(counter_);
  }

  v8::Handle<v8::Value> Increment(const v8::Arguments& args) {
    counter_++;
    return v8::Undefined();
  }

 private:
  int counter_;
};

class TestExceptionHandler : public ModuleSystem::ExceptionHandler {
 public:
  TestExceptionHandler()
      : handled_exception_(false) {
  }

  virtual void HandleUncaughtException() OVERRIDE {
    handled_exception_ = true;
  }

  bool handled_exception() const { return handled_exception_; }

 private:
  bool handled_exception_;
};

TEST_F(ModuleSystemTest, TestExceptionHandling) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  TestExceptionHandler* handler = new TestExceptionHandler;
  scoped_ptr<ModuleSystem::ExceptionHandler> scoped_handler(handler);
  ASSERT_FALSE(handler->handled_exception());
  module_system_->set_exception_handler(scoped_handler.Pass());

  RegisterModule("test", "throw 'hi';");
  module_system_->Require("test");
  ASSERT_TRUE(handler->handled_exception());

  ExpectNoAssertionsMade();
}

TEST_F(ModuleSystemTest, TestRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("add", "exports.Add = function(x, y) { return x + y; };");
  RegisterModule("test",
      "var Add = require('add').Add;"
      "requireNative('assert').AssertTrue(Add(3, 5) == 8);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestNestedRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("add", "exports.Add = function(x, y) { return x + y; };");
  RegisterModule("double",
      "var Add = require('add').Add;"
      "exports.Double = function(x) { return Add(x, x); };");
  RegisterModule("test",
      "var Double = require('double').Double;"
      "requireNative('assert').AssertTrue(Double(3) == 6);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestModuleInsulation) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("x",
      "var x = 10;"
      "exports.X = function() { return x; };");
  RegisterModule("y",
      "var x = 15;"
      "require('x');"
      "exports.Y = function() { return x; };");
  RegisterModule("test",
      "var Y = require('y').Y;"
      "var X = require('x').X;"
      "var assert = requireNative('assert');"
      "assert.AssertTrue(!this.hasOwnProperty('x'));"
      "assert.AssertTrue(Y() == 15);"
      "assert.AssertTrue(X() == 10);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestNativesAreDisabledOutsideANativesEnabledScope) {
  RegisterModule("test",
      "var assert;"
      "try {"
      "  assert = requireNative('assert');"
      "} catch (e) {"
      "  var caught = true;"
      "}"
      "if (assert) {"
      "  assert.AssertTrue(true);"
      "}");
  module_system_->Require("test");
  ExpectNoAssertionsMade();
}

TEST_F(ModuleSystemTest, TestNativesAreEnabledWithinANativesEnabledScope) {
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(true);");

  {
    ModuleSystem::NativesEnabledScope natives_enabled(module_system_.get());
    {
      ModuleSystem::NativesEnabledScope natives_enabled_inner(
          module_system_.get());
    }
    module_system_->Require("test");
  }
}

TEST_F(ModuleSystemTest, TestLazyField) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("lazy",
      "exports.x = 5;");

  v8::Handle<v8::Object> object = CreateGlobal("object");

  module_system_->SetLazyField(object, "blah", "lazy", "x");

  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(object.blah == 5);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyFieldYieldingObject) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("lazy",
      "var object = {};"
      "object.__defineGetter__('z', function() { return 1; });"
      "object.x = 5;"
      "object.y = function() { return 10; };"
      "exports.object = object;");

  v8::Handle<v8::Object> object = CreateGlobal("object");

  module_system_->SetLazyField(object, "thing", "lazy", "object");

  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(object.thing.x == 5);"
      "assert.AssertTrue(object.thing.y() == 10);"
      "assert.AssertTrue(object.thing.z == 1);"
      );
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyFieldIsOnlyEvaledOnce) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  module_system_->RegisterNativeHandler(
      "counter",
      scoped_ptr<NativeHandler>(new CounterNatives(v8::Isolate::GetCurrent())));
  RegisterModule("lazy",
      "requireNative('counter').Increment();"
      "exports.x = 5;");

  v8::Handle<v8::Object> object = CreateGlobal("object");

  module_system_->SetLazyField(object, "x", "lazy", "x");

  RegisterModule("test",
      "var assert = requireNative('assert');"
      "var counter = requireNative('counter');"
      "assert.AssertTrue(counter.Get() == 0);"
      "object.x;"
      "assert.AssertTrue(counter.Get() == 1);"
      "object.x;"
      "assert.AssertTrue(counter.Get() == 1);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestRequireNativesAfterLazyEvaluation) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("lazy",
      "exports.x = 5;");
  v8::Handle<v8::Object> object = CreateGlobal("object");

  module_system_->SetLazyField(object, "x", "lazy", "x");
  RegisterModule("test",
      "object.x;"
      "requireNative('assert').AssertTrue(true);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestTransitiveRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("dependency",
      "exports.x = 5;");
  RegisterModule("lazy",
      "exports.output = require('dependency');");

  v8::Handle<v8::Object> object = CreateGlobal("object");

  module_system_->SetLazyField(object, "thing", "lazy", "output");

  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(object.thing.x == 5);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestModulesOnlyGetEvaledOnce) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  module_system_->RegisterNativeHandler(
      "counter",
      scoped_ptr<NativeHandler>(new CounterNatives(v8::Isolate::GetCurrent())));

  RegisterModule("incrementsWhenEvaled",
      "requireNative('counter').Increment();");
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "var counter = requireNative('counter');"
      "assert.AssertTrue(counter.Get() == 0);"
      "require('incrementsWhenEvaled');"
      "assert.AssertTrue(counter.Get() == 1);"
      "require('incrementsWhenEvaled');"
      "assert.AssertTrue(counter.Get() == 1);");

  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestOverrideNativeHandler) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  OverrideNativeHandler("assert", "exports.AssertTrue = function() {};");
  RegisterModule("test", "requireNative('assert').AssertTrue(true);");
  ExpectNoAssertionsMade();
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestOverrideNonExistentNativeHandler) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  OverrideNativeHandler("thing", "exports.x = 5;");
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(requireNative('thing').x == 5);");
  module_system_->Require("test");
}
