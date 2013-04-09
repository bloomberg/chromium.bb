// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/permission_feature.h"

#include "chrome/common/extensions/permissions/permission_set.h"

namespace extensions {

PermissionFeature::PermissionFeature() {
}

PermissionFeature::~PermissionFeature() {
}

Feature::Availability PermissionFeature::IsAvailableToContext(
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

  // Optional permissions need to be checked so an API will not be set to
  // undefined forever, when it could just need optional permissions.
  if (extension && !extension->HasAPIPermission(name()) &&
      !extension->optional_permission_set()->HasAnyAccessToAPI(name())) {
    return CreateAvailability(NOT_PRESENT, extension->GetType());
  }

  return CreateAvailability(IS_AVAILABLE);
}

std::string PermissionFeature::Parse(const DictionaryValue* value) {
  std::string error = SimpleFeature::Parse(value);
  if (!error.empty())
    return error;

  if (extension_types()->empty()) {
    return name() + ": Permission features must specify at least one " +
        "value for extension_types.";
  }

  if (!GetContexts()->empty())
    return name() + ": Permission features do not support contexts.";

  return "";
}

}  // namespace
