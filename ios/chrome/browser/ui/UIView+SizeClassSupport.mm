// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

#import "base/logging.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/ui_util.h"

namespace {

// Returns the SizeClassIdiom corresponding with |size_class|.
SizeClassIdiom GetSizeClassIdiom(UIUserInterfaceSizeClass size_class) {
  switch (size_class) {
    case UIUserInterfaceSizeClassCompact:
      return COMPACT;
    case UIUserInterfaceSizeClassRegular:
      return REGULAR;
    case UIUserInterfaceSizeClassUnspecified:
      return UNSPECIFIED;
  }
}

// Returns YES if |size_class| is not UIUserInterfaceSizeClassUnspecified.
bool IsSizeClassSpecified(UIUserInterfaceSizeClass size_class) {
  return size_class != UIUserInterfaceSizeClassUnspecified;
}

// The height of an iPhone 6 in portrait.  A UIWindow's size class idiom is
// REGULAR if the frame's size is greater than this value in that dimension.
const CGFloat kIPhone6PortraitHeight = 667.0f;

// Returns the SizeClassIdiom for a UIWindow with a side of |side_length| points
// in a given dimension.  This fallback approach is used when:
// - |-traitCollection| is not implemented (i.e. pre-iOS8), or
// - both the target and the UIApplication's |-keyWindow| have an unspecified
//   size class.
SizeClassIdiom SizeClassForSideLength(CGFloat side_length) {
  return side_length > kIPhone6PortraitHeight ? REGULAR : COMPACT;
}

}  // namespace

@implementation UIView (SizeClassSupport)

- (SizeClassIdiom)cr_widthSizeClass {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  if ([self respondsToSelector:@selector(traitCollection)]) {
    UIUserInterfaceSizeClass sizeClass =
        self.traitCollection.horizontalSizeClass;
    if (!IsSizeClassSpecified(sizeClass))
      sizeClass = keyWindow.traitCollection.horizontalSizeClass;
    if (IsSizeClassSpecified(sizeClass))
      return GetSizeClassIdiom(sizeClass);
    LOG(WARNING) << "Encountered UIWindow with unspecified width size class.";
  }
  return SizeClassForSideLength(CGRectGetWidth(keyWindow.frame));
}

- (SizeClassIdiom)cr_heightSizeClass {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  if ([self respondsToSelector:@selector(traitCollection)]) {
    UIUserInterfaceSizeClass sizeClass = self.traitCollection.verticalSizeClass;
    if (!IsSizeClassSpecified(sizeClass))
      sizeClass = keyWindow.traitCollection.verticalSizeClass;
    if (IsSizeClassSpecified(sizeClass))
      return GetSizeClassIdiom(sizeClass);
    LOG(WARNING) << "Encountered UIWindow with unspecified height size class.";
  }
  return SizeClassForSideLength(CGRectGetHeight(keyWindow.frame));
}

@end
