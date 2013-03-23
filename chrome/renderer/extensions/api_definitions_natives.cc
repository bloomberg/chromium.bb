// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api_definitions_natives.h"

#include <algorithm>

#include "chrome/common/extensions/api/extension_api.h"

namespace {
const char kInvalidExtensionNamespace[] = "Invalid extension namespace";
}

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(Dispatcher* dispatcher,
                                             ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context->v8_context()),
      context_(context) {
  RouteFunction("GetExtensionAPIDefinitions",
                base::Bind(&ApiDefinitionsNatives::GetExtensionAPIDefinitions,
                           base::Unretained(this)));
}

v8::Handle<v8::Value> ApiDefinitionsNatives::GetExtensionAPIDefinitions(
    const v8::Arguments& args) {
  return dispatcher()->v8_schema_registry()->GetSchemas(
      ExtensionAPI::GetSharedInstance()->GetAllAPINames());
}

}  // namespace extensions
