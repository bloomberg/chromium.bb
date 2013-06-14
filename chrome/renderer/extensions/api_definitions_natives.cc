// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api_definitions_natives.h"

#include <algorithm>

#include "chrome/common/extensions/features/base_feature_provider.h"

namespace {
const char kInvalidExtensionNamespace[] = "Invalid extension namespace";
}

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(Dispatcher* dispatcher,
                                             ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetExtensionAPIDefinitionsForTest",
                base::Bind(
                    &ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest,
                    base::Unretained(this)));
}

v8::Handle<v8::Value> ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest(
    const v8::Arguments& args) {
  std::vector<std::string> apis;
  FeatureProvider* feature_provider = BaseFeatureProvider::GetByName("api");
  const std::vector<std::string>& feature_names =
      feature_provider->GetAllFeatureNames();
  for (std::vector<std::string>::const_iterator i = feature_names.begin();
       i != feature_names.end(); ++i) {
    if (!feature_provider->GetParent(feature_provider->GetFeature(*i)) &&
        context()->GetAvailability(*i).is_available()) {
      apis.push_back(*i);
    }
  }
  return dispatcher()->v8_schema_registry()->GetSchemas(apis);
}

}  // namespace extensions
