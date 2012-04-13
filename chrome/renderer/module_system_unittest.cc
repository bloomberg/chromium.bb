// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/renderer/module_system.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <map>
#include <string>

// Native JS functions for doing asserts.
class AssertNatives : public NativeHandler {
 public:
  AssertNatives()
      : assertion_made_(false),
        failed_(false) {
    RouteFunction("AssertTrue", base::Bind(&AssertNatives::AssertTrue,
        base::Unretained(this)));
  }

  bool assertion_made() { return assertion_made_; }
  bool failed() { return failed_; }

  v8::Handle<v8::Value> AssertTrue(const v8::Arguments& args) {
    assertion_made_ = true;
    failed_ = failed_ || !args[0]->ToBoolean()->Value();
    return v8::Undefined();
  }

 private:
  bool assertion_made_;
  bool failed_;
};

class CounterNatives : public NativeHandler {
 public:
  CounterNatives() : counter_(0) {
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

class StringSourceMap : public ModuleSystem::SourceMap {
 public:
  StringSourceMap() {}
  virtual ~StringSourceMap() {}

  v8::Handle<v8::Value> GetSource(const std::string& name) OVERRIDE {
    if (source_map_.count(name) == 0)
      return v8::Undefined();
    return v8::String::New(source_map_[name].c_str());
  }

  bool Contains(const std::string& name) OVERRIDE {
    return source_map_.count(name);
  }

  void RegisterModule(const std::string& name, const std::string& source) {
    source_map_[name] = source;
  }

 private:
  std::map<std::string, std::string> source_map_;
};

class ModuleSystemTest : public testing::Test {
 public:
  ModuleSystemTest()
      : context_(v8::Context::New()),
        source_map_(new StringSourceMap()),
        should_assertions_be_made_(true) {
    context_->Enter();
    assert_natives_ = new AssertNatives();
    module_system_.reset(new ModuleSystem(context_, source_map_.get()));
    module_system_->RegisterNativeHandler("assert", scoped_ptr<NativeHandler>(
        assert_natives_));
    RegisterModule("add", "exports.Add = function(x, y) { return x + y; };");
  }

  ~ModuleSystemTest() {
    module_system_.reset();
    context_->Exit();
    context_.Dispose();
  }

  void RegisterModule(const std::string& name, const std::string& code) {
    source_map_->RegisterModule(name, code);
  }

  virtual void TearDown() {
    ASSERT_FALSE(try_catch_.HasCaught());
    // All tests must assert at least once unless otherwise specified.
    ASSERT_EQ(should_assertions_be_made_,
              assert_natives_->assertion_made());
    ASSERT_FALSE(assert_natives_->failed());
  }

  void ExpectNoAssertionsMade() {
    should_assertions_be_made_ = false;
  }

  v8::Handle<v8::Object> CreateGlobal(const std::string& name) {
    v8::HandleScope handle_scope;
    v8::Handle<v8::Object> object = v8::Object::New();
    v8::Context::GetCurrent()->Global()->Set(v8::String::New(name.c_str()),
                                             object);
    return handle_scope.Close(object);
  }

  v8::Persistent<v8::Context> context_;
  v8::HandleScope handle_scope_;
  v8::TryCatch try_catch_;
  AssertNatives* assert_natives_;
  scoped_ptr<StringSourceMap> source_map_;
  scoped_ptr<ModuleSystem> module_system_;
  bool should_assertions_be_made_;
};

TEST_F(ModuleSystemTest, TestRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var Add = require('add').Add;"
      "requireNative('assert').AssertTrue(Add(3, 5) == 8);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestNestedRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
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
      "  caught = true;"
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
      scoped_ptr<NativeHandler>(new CounterNatives()));
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
      scoped_ptr<NativeHandler>(new CounterNatives()));

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
