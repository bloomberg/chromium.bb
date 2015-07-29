// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

namespace {

SizeClassIdiom GetSizeClassIdiom(UIUserInterfaceSizeClass size_class) {
  return size_class == UIUserInterfaceSizeClassCompact ? COMPACT : REGULAR;
}

}  // namespace

@implementation UIView (SizeClassSupport)

- (SizeClassIdiom)cr_widthSizeClass {
  return GetSizeClassIdiom(self.traitCollection.horizontalSizeClass);
}

- (SizeClassIdiom)cr_heightSizeClass {
  return GetSizeClassIdiom(self.traitCollection.verticalSizeClass);
}

@end
