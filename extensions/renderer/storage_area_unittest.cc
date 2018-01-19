// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/storage_area.h"

#include "extensions/common/extension_builder.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using StorageAreaTest = NativeExtensionBindingsSystemUnittest;

TEST_F(StorageAreaTest, TestUseAfterInvalidation) {
  scoped_refptr<Extension> extension =
      ExtensionBuilder("foo").AddPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage.local");
  ASSERT_TRUE(storage->IsObject());

  constexpr char kRunStorageGet[] =
      "(function(storage) { storage.get('foo', function() {}); })";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage};
  RunFunction(run_storage_get, context, arraysize(args), args);

  DisposeContext(context);

  EXPECT_FALSE(binding::IsContextValid(context));
  RunFunctionAndExpectError(run_storage_get, context, arraysize(args), args,
                            "Uncaught Error: Extension context invalidated.");
}

}  // namespace extensions
