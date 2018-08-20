// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_api_provider.h"

#include "chrome/common/extensions/api/api_features.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/api/manifest_features.h"
#include "chrome/common/extensions/api/permission_features.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/features/json_feature_provider_source.h"

namespace extensions {

ChromeExtensionsAPIProvider::ChromeExtensionsAPIProvider() = default;
ChromeExtensionsAPIProvider::~ChromeExtensionsAPIProvider() = default;

void ChromeExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddChromeAPIFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddManifestFeatures(
    FeatureProvider* provider) {
  AddChromeManifestFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddChromePermissionFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddBehaviorFeatures(
    FeatureProvider* provider) {
  // Note: No chrome-specific behavior features.
}

void ChromeExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROME_EXTENSION_API_FEATURES);
}

bool ChromeExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return api::ChromeGeneratedSchemas::IsGenerated(name);
}

base::StringPiece ChromeExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::ChromeGeneratedSchemas::Get(name);
}

}  // namespace extensions
