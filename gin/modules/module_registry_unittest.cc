// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_registry.h"

#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8.h"

namespace gin {

typedef V8Test ModuleRegistryTest;

// Verifies ModuleRegistry is not available after ContextHolder has been
// deleted.
TEST_F(ModuleRegistryTest, DestroyedWithContext) {
  v8::Isolate::Scope isolate_scope(instance_->isolate());
  v8::HandleScope handle_scope(instance_->isolate());
  v8::Handle<v8::Context> context = v8::Context::New(
      instance_->isolate(), NULL, v8::Handle<v8::ObjectTemplate>());
  {
    ContextHolder context_holder(instance_->isolate());
    context_holder.SetContext(context);
    ModuleRegistry* registry = ModuleRegistry::From(context);
    EXPECT_TRUE(registry != NULL);
  }
  ModuleRegistry* registry = ModuleRegistry::From(context);
  EXPECT_TRUE(registry == NULL);
}

}  // namespace gin
