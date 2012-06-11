// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_PERMISSION_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_PERMISSION_FEATURE_H_
#pragma once

#include "chrome/common/extensions/features/feature.h"

namespace extensions {

class PermissionFeature : public Feature {
 public:
  PermissionFeature();
  virtual ~PermissionFeature();

  virtual Feature::Availability IsAvailableToContext(
      const Extension* extension,
      Feature::Context context,
      Feature::Platform platform) const OVERRIDE;
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_PERMISSION_FEATURE_H_
