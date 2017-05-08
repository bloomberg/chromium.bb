// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_TEST_H_
#define EXTENSIONS_RENDERER_API_BINDING_TEST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace gin {
class ContextHolder;
class IsolateHolder;
}

namespace extensions {

// A common unit test class for testing API bindings. Creates an isolate and an
// initial v8 context, and checks for v8 leaks at the end of the test.
class APIBindingTest : public testing::Test {
 protected:
  APIBindingTest();
  ~APIBindingTest() override;

  // Returns the V8 ExtensionConfiguration to use for contexts. The default
  // implementation returns null.
  virtual v8::ExtensionConfiguration* GetV8ExtensionConfiguration();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Returns a local to the main context.
  v8::Local<v8::Context> MainContext();

  // Creates a new context (maintaining it with a holder in
  // |additional_context_holders_| and returns a local.
  v8::Local<v8::Context> AddContext();

  // Disposes the context pointed to by |context|.
  void DisposeContext(v8::Local<v8::Context> context);

  // Disposes all contexts and checks for leaks.
  void DisposeAllContexts();

  // Allows subclasses to perform context disposal cleanup.
  virtual void OnWillDisposeContext(v8::Local<v8::Context> context) {}

  // Returns the associated isolate. Defined out-of-line to avoid the include
  // for IsolateHolder in the header.
  v8::Isolate* isolate();

 private:
  void RunGarbageCollection();

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> main_context_holder_;
  std::vector<std::unique_ptr<gin::ContextHolder>> additional_context_holders_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_TEST_H_
