// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MODULE_SYSTEM_TEST_H_
#define CHROME_TEST_BASE_MODULE_SYSTEM_TEST_H_
#pragma once

#include "chrome/renderer/module_system.h"
#include "v8/include/v8.h"
#include "testing/gtest/include/gtest/gtest.h"

class AssertNatives;
class StringSourceMap;

// Test fixture for testing JS that makes use of the module system.
//
// Typically tests will look like:
//
// TEST_F(MyModuleSystemTest, TestStuff) {
//   ModuleSystem::NativesEnabledScope natives_enabled(module_system_.get());
//   RegisterModule("test", "requireNative('assert').AssertTrue(true);");
//   module_system_->Require("test");
// }
//
// By default a test will fail if no method in the native module 'assert' is
// called. This behaviour can be overridden by calling ExpectNoAssertionsMade().
class ModuleSystemTest : public testing::Test {
 public:
  ModuleSystemTest();
  virtual ~ModuleSystemTest();

  virtual void TearDown() OVERRIDE;

 protected:
  // Register a named JS module in the module system.
  void RegisterModule(const std::string& name, const std::string& code);

  // Register a named JS module with source retrieved from a ResourceBundle.
  void RegisterModule(const std::string& name, int resource_id);

  // Register a named JS module in the module system and tell the module system
  // to use it to handle any requireNative() calls for native modules with that
  // name.
  void OverrideNativeHandler(const std::string& name, const std::string& code);

  // Make the test fail if any asserts are called. By default a test will fail
  // if no asserts are called.
  void ExpectNoAssertionsMade();

  // Create an empty object in the global scope with name |name|.
  v8::Handle<v8::Object> CreateGlobal(const std::string& name);

  v8::Persistent<v8::Context> context_;
  v8::HandleScope handle_scope_;
  v8::TryCatch try_catch_;
  AssertNatives* assert_natives_;
  scoped_ptr<StringSourceMap> source_map_;
  scoped_ptr<ModuleSystem> module_system_;
  bool should_assertions_be_made_;
};

#endif  // CHROME_TEST_BASE_MODULE_SYSTEM_TEST_H_
