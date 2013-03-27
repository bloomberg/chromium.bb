// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/manifest_feature.h"

#include "chrome/common/extensions/manifest.h"

namespace extensions {

ManifestFeature::ManifestFeature() {
}

ManifestFeature::~ManifestFeature() {
}

Feature::Availability ManifestFeature::IsAvailableToContext(
    const Extension* extension,
    Feature::Context context,
    const GURL& url,
    Feature::Platform platform) const {
  Availability availability = SimpleFeature::IsAvailableToContext(extension,
                                                                  context,
                                                                  url,
                                                                  platform);
  if (!availability.is_available())
    return availability;

  // We know we can skip manifest()->GetKey() here because we just did the same
  // validation it would do above.
  if (extension && !extension->manifest()->value()->HasKey(name()))
    return CreateAvailability(NOT_PRESENT, extension->GetType());

  return CreateAvailability(IS_AVAILABLE);
}

std::string ManifestFeature::Parse(const DictionaryValue* value) {
  std::string error = SimpleFeature::Parse(value);
  if (!error.empty())
    return error;

  if (extension_types()->empty()) {
    return name() + ": Manifest features must specify at least one " +
        "value for extension_types.";
  }

  if (!GetContexts()->empty())
    return name() + ": Manifest features do not support contexts.";

  return "";
}

}  // namespace
