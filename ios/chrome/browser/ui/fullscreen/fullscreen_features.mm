// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace fullscreen {
namespace features {

const base::Feature kNewFullscreen{"NewFullscreen",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace fullscreen
