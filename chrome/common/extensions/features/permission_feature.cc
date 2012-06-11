// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/permission_feature.h"

#include "chrome/common/extensions/extension_permission_set.h"

namespace extensions {

PermissionFeature::PermissionFeature() {
}

PermissionFeature::~PermissionFeature() {
}

Feature::Availability PermissionFeature::IsAvailableToContext(
    const Extension* extension,
    Feature::Context context,
    Feature::Platform platform) const {
  Availability availability = Feature::IsAvailableToContext(extension,
                                                            context,
                                                            platform);
  if (availability != IS_AVAILABLE)
    return availability;

  if (!extension->HasAPIPermission(name()))
    return NOT_PRESENT;

  return IS_AVAILABLE;
}

}  // namespace
