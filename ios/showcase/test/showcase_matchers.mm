// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace showcase_matchers {

id<GREYMatcher> FirstLevelBackButton() {
  return grey_kindOfClass(
      NSClassFromString(@"_UINavigationBarBackIndicatorView"));
}

}  // namespace showcase_matchers
