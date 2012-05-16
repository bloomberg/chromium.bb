// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api_definitions_natives.h"

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(extension_dispatcher) {
  RouteFunction("GetExtensionAPIDefinition",
                base::Bind(&ApiDefinitionsNatives::GetExtensionAPIDefinition,
                           base::Unretained(this)));
}

v8::Handle<v8::Value> ApiDefinitionsNatives::GetExtensionAPIDefinition(
    const v8::Arguments& args) {
  ChromeV8Context* v8_context =
      extension_dispatcher()->v8_context_set().GetCurrent();
  CHECK(v8_context);
  return extension_dispatcher()->v8_schema_registry()->GetSchemas(
      v8_context->GetAvailableExtensionAPIs());
}

}  // namespace extensions
