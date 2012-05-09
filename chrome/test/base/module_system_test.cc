// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/module_system_test.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/renderer/native_handler.h"

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
    RouteFunction("AssertFalse", base::Bind(&AssertNatives::AssertFalse,
        base::Unretained(this)));
  }

  bool assertion_made() { return assertion_made_; }
  bool failed() { return failed_; }

  v8::Handle<v8::Value> AssertTrue(const v8::Arguments& args) {
    CHECK_EQ(1, args.Length());
    assertion_made_ = true;
    failed_ = failed_ || !args[0]->ToBoolean()->Value();
    return v8::Undefined();
  }

  v8::Handle<v8::Value> AssertFalse(const v8::Arguments& args) {
    CHECK_EQ(1, args.Length());
    assertion_made_ = true;
    failed_ = failed_ || args[0]->ToBoolean()->Value();
    return v8::Undefined();
  }

 private:
  bool assertion_made_;
  bool failed_;
};

// Source map that operates on std::strings.
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
    CHECK_EQ(0u, source_map_.count(name));
    source_map_[name] = source;
  }

 private:
  std::map<std::string, std::string> source_map_;
};

ModuleSystemTest::ModuleSystemTest()
    : context_(v8::Context::New()),
      source_map_(new StringSourceMap()),
      should_assertions_be_made_(true) {
  context_->Enter();
  assert_natives_ = new AssertNatives();
  module_system_.reset(new ModuleSystem(context_, source_map_.get()));
  module_system_->RegisterNativeHandler("assert", scoped_ptr<NativeHandler>(
      assert_natives_));
  try_catch_.SetCaptureMessage(true);
}

ModuleSystemTest::~ModuleSystemTest() {
  module_system_.reset();
  context_->Exit();
  context_.Dispose();
}

void ModuleSystemTest::RegisterModule(const std::string& name,
                                      const std::string& code) {
  source_map_->RegisterModule(name, code);
}

void ModuleSystemTest::TearDown() {
  EXPECT_FALSE(try_catch_.HasCaught());
  // All tests must assert at least once unless otherwise specified.
  EXPECT_EQ(should_assertions_be_made_,
            assert_natives_->assertion_made());
  EXPECT_FALSE(assert_natives_->failed());
}

void ModuleSystemTest::ExpectNoAssertionsMade() {
  should_assertions_be_made_ = false;
}

v8::Handle<v8::Object> ModuleSystemTest::CreateGlobal(const std::string& name) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Object> object = v8::Object::New();
  v8::Context::GetCurrent()->Global()->Set(v8::String::New(name.c_str()),
                                           object);
  return handle_scope.Close(object);
}
