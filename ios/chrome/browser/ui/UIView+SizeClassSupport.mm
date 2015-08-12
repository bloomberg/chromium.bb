// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/legacy_size_class_support_util.h"

namespace {

SizeClassIdiom GetSizeClassIdiom(UIUserInterfaceSizeClass size_class) {
  return size_class == UIUserInterfaceSizeClassCompact ? COMPACT : REGULAR;
}

}  // namespace

@implementation UIView (SizeClassSupport)

- (SizeClassIdiom)cr_widthSizeClass {
  if (base::ios::IsRunningOnIOS8OrLater())
    return GetSizeClassIdiom(self.traitCollection.horizontalSizeClass);
  return CurrentWidthSizeClass();
}

- (SizeClassIdiom)cr_heightSizeClass {
  if (base::ios::IsRunningOnIOS8OrLater())
    return GetSizeClassIdiom(self.traitCollection.verticalSizeClass);
  return CurrentHeightSizeClass();
}

@end
