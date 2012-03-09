// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/module_system.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <map>
#include <string>

class AssertNatives : public NativeHandler {
 public:
  AssertNatives()
      : native_function_called_(false),
        failed_(false) {
    RouteFunction("AssertTrue", base::Bind(&AssertNatives::AssertTrue,
        base::Unretained(this)));
  }

  bool native_function_called() { return native_function_called_; }
  bool failed() { return failed_; }

  v8::Handle<v8::Value> AssertTrue(const v8::Arguments& args) {
    native_function_called_ = true;
    failed_ = failed_ || !args[0]->ToBoolean()->Value();
    return v8::Undefined();
  }

 private:
  bool native_function_called_;
  bool failed_;
};

class ModuleSystemTest : public testing::Test {
 public:
  ModuleSystemTest()
      : context_(v8::Context::New()),
        assert_natives_(new AssertNatives()) {
    context_->Enter();
    source_map_["add"] = "exports.Add = function(x, y) { return x + y; };";
    module_system_.reset(new ModuleSystem(&source_map_));
    module_system_->RegisterNativeHandler("assert", scoped_ptr<NativeHandler>(
        assert_natives_));
  }

  ~ModuleSystemTest() {
    context_.Dispose();
  }

  virtual void TearDown() {
    // All tests must call a native function at least once.
    ASSERT_TRUE(assert_natives_->native_function_called());
    ASSERT_FALSE(assert_natives_->failed());
    ASSERT_FALSE(try_catch_.HasCaught());
  }

  v8::HandleScope handle_scope_;
  v8::TryCatch try_catch_;
  v8::Persistent<v8::Context> context_;
  AssertNatives* assert_natives_;
  std::map<std::string, std::string> source_map_;
  scoped_ptr<ModuleSystem> module_system_;
};

TEST_F(ModuleSystemTest, TestRequire) {
  source_map_["test"] =
      "var Add = require('add').Add;"
      "requireNative('assert').AssertTrue(Add(3, 5) == 8);";
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestNestedRequire) {
  source_map_["double"] =
      "var Add = require('add').Add;"
      "exports.Double = function(x) { return Add(x, x); };";
  source_map_["test"] =
      "var Double = require('double').Double;"
      "requireNative('assert').AssertTrue(Double(3) == 6);";
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestModuleInsulation) {
  source_map_["x"] =
      "var x = 10;"
      "exports.X = function() { return x; };";
  source_map_["y"] =
      "var x = 15;"
      "require('x');"
      "exports.Y = function() { return x; };";
  source_map_["test"] =
      "var Y = require('y').Y;"
      "var X = require('x').X;"
      "var assert = requireNative('assert');"
      "assert.AssertTrue(!this.hasOwnProperty('x'));"
      "assert.AssertTrue(Y() == 15);"
      "assert.AssertTrue(X() == 10);";
  module_system_->Require("test");
}
