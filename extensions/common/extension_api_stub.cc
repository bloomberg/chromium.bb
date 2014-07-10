// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub methods to be used when extensions are disabled
// i.e. ENABLE_EXTENSIONS is not defined

#include "extensions/common/extension_api.h"

#include "extensions/common/features/feature.h"

namespace extensions {

// static
ExtensionAPI* ExtensionAPI::GetSharedInstance() {
  return NULL;
}

// static
ExtensionAPI* ExtensionAPI::CreateWithDefaultConfiguration() {
  return NULL;
}

Feature::Availability ExtensionAPI::IsAvailable(
    const std::string& api_full_name,
    const Extension* extension,
    Feature::Context context,
    const GURL& url) {
  return Feature::CreateAvailability(Feature::NOT_PRESENT, "");
}

bool ExtensionAPI::IsAnyFeatureAvailableToContext(const Feature& api,
                                                  const Extension* extension,
                                                  Feature::Context context,
                                                  const GURL& url) {
  return false;
}

bool ExtensionAPI::IsAvailableInUntrustedContext(const std::string& full_name,
                                                 const Extension* extension) {
  return false;
}

const base::DictionaryValue* ExtensionAPI::GetSchema(
    const std::string& full_name) {
  return NULL;
}

Feature* ExtensionAPI::GetFeatureDependency(const std::string& name) {
  return NULL;
}

}  // namespace extensions
