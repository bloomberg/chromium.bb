// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api_definitions_natives.h"

#include <algorithm>

namespace {
const char kInvalidExtensionNamespace[] = "Invalid extension namespace";
}

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(Dispatcher* dispatcher)
    : ChromeV8Extension(dispatcher) {
  RouteFunction("GetExtensionAPIDefinition",
                base::Bind(&ApiDefinitionsNatives::GetExtensionAPIDefinition,
                           base::Unretained(this)));
}

v8::Handle<v8::Value> ApiDefinitionsNatives::GetExtensionAPIDefinition(
    const v8::Arguments& args) {
  ChromeV8Context* v8_context = dispatcher()->v8_context_set().GetCurrent();
  CHECK(v8_context);

  std::set<std::string> available_apis(v8_context->GetAvailableExtensionAPIs());
  if (args.Length() == 0)
    return dispatcher()->v8_schema_registry()->GetSchemas(available_apis);

  // Build set of APIs requested by the user.
  std::set<std::string> requested_apis;
  for (int i = 0; i < args.Length(); ++i) {
    if (!args[i]->IsString()) {
      v8::ThrowException(v8::String::New(kInvalidExtensionNamespace));
      return v8::Undefined();
    }
    requested_apis.insert(*v8::String::Utf8Value(args[i]->ToString()));
  }

  // Filter those that are unknown.
  std::set<std::string> apis_to_check;
  std::set_intersection(requested_apis.begin(), requested_apis.end(),
                        available_apis.begin(), available_apis.end(),
                        std::inserter(apis_to_check, apis_to_check.begin()));
  return dispatcher()->v8_schema_registry()->GetSchemas(apis_to_check);
}

}  // namespace extensions
