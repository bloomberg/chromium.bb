// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gc_callback.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/scoped_web_frame.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "gin/function_template.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

namespace extensions {
namespace {

void SetToTrue(bool* value) {
  if (*value)
    ADD_FAILURE() << "Value is already true";
  *value = true;
}

class GCCallbackTest : public testing::Test {
 public:
  GCCallbackTest() : script_context_set_(&active_extensions_) {}

 protected:
  ScriptContextSet& script_context_set() { return script_context_set_; }

  v8::Local<v8::Context> v8_context() {
    return v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), v8_context_);
  }

  ScriptContext* RegisterScriptContext() {
    // No world ID.
    return script_context_set_.Register(
        web_frame_.frame(),
        v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), v8_context_), 0);
  }

  void RequestGarbageCollection() {
    v8::Isolate::GetCurrent()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }

 private:
  void SetUp() override {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handle_scope(isolate);
    // We need a context that has been initialized by blink; grab the main world
    // context from the web frame.
    v8::Local<v8::Context> local_v8_context =
        web_frame_.frame()->MainWorldScriptContext();
    DCHECK(!local_v8_context.IsEmpty());
    v8_context_.Reset(isolate, local_v8_context);
  }

  void TearDown() override {
    v8_context_.Reset();
    RequestGarbageCollection();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  ScopedWebFrame web_frame_;  // (this will construct the v8::Isolate)
  // ExtensionsRendererClient is a dependency of ScriptContextSet.
  TestExtensionsRendererClient extensions_renderer_client_;
  ExtensionIdSet active_extensions_;
  ScriptContextSet script_context_set_;
  v8::Global<v8::Context> v8_context_;

  DISALLOW_COPY_AND_ASSIGN(GCCallbackTest);
};

TEST_F(GCCallbackTest, GCBeforeContextInvalidated) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(v8_context());

  ScriptContext* script_context = RegisterScriptContext();

  bool callback_invoked = false;
  bool fallback_invoked = false;

  {
    // Nest another HandleScope so that |object| and |unreachable_function|'s
    // handles will be garbage collected.
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::FunctionTemplate> unreachable_function =
        gin::CreateFunctionTemplate(isolate,
                                    base::Bind(SetToTrue, &callback_invoked));
    // The GCCallback will delete itself, or memory tests will complain.
    new GCCallback(script_context, object, unreachable_function->GetFunction(),
                   base::Bind(SetToTrue, &fallback_invoked));
  }

  // Trigger a GC. Only the callback should be invoked.
  RequestGarbageCollection();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_invoked);
  EXPECT_FALSE(fallback_invoked);

  // Invalidate the context. The fallback should not be invoked because the
  // callback was already invoked.
  script_context_set().Remove(script_context);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fallback_invoked);
}

TEST_F(GCCallbackTest, ContextInvalidatedBeforeGC) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(v8_context());

  ScriptContext* script_context = RegisterScriptContext();

  bool callback_invoked = false;
  bool fallback_invoked = false;

  {
    // Nest another HandleScope so that |object| and |unreachable_function|'s
    // handles will be garbage collected.
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::FunctionTemplate> unreachable_function =
        gin::CreateFunctionTemplate(isolate,
                                    base::Bind(SetToTrue, &callback_invoked));
    // The GCCallback will delete itself, or memory tests will complain.
    new GCCallback(script_context, object, unreachable_function->GetFunction(),
                   base::Bind(SetToTrue, &fallback_invoked));
  }

  // Invalidate the context. Only the fallback should be invoked.
  script_context_set().Remove(script_context);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(callback_invoked);
  EXPECT_TRUE(fallback_invoked);

  // Trigger a GC. The callback should not be invoked because the fallback was
  // already invoked.
  RequestGarbageCollection();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(callback_invoked);
}

}  // namespace
}  // namespace extensions
