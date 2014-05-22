// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_definitions_natives.h"

#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(Dispatcher* dispatcher,
                                             ScriptContext* context)
    : ObjectBackedNativeHandler(context), dispatcher_(dispatcher) {
  RouteFunction(
      "GetExtensionAPIDefinitionsForTest",
      base::Bind(&ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest,
                 base::Unretained(this)));
}

void ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::vector<std::string> apis;
  const FeatureProvider* feature_provider = FeatureProvider::GetAPIFeatures();
  const std::vector<std::string>& feature_names =
      feature_provider->GetAllFeatureNames();
  for (std::vector<std::string>::const_iterator i = feature_names.begin();
       i != feature_names.end();
       ++i) {
    if (!feature_provider->GetParent(feature_provider->GetFeature(*i)) &&
        context()->GetAvailability(*i).is_available()) {
      apis.push_back(*i);
    }
  }
  args.GetReturnValue().Set(
      dispatcher_->v8_schema_registry()->GetSchemas(apis));
}

}  // namespace extensions
