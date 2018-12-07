// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/download/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace download {

// Controls previewing USDZ format 3D models in AR Quick Look.
const base::Feature kUsdzPreview{"USDZPreview",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Returns whether AR Quick Look in enabled.
bool IsUsdzPreviewEnabled() {
  if (@available(iOS 12, *)) {
    return base::FeatureList::IsEnabled(kUsdzPreview);
  }
  return false;
}

}  // namespace download
