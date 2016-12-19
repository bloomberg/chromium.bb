// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_extended_button.h"

#include "base/logging.h"

namespace {
// Apple's guidelines indicate that buttons should have a minimum touch area of
// 44pt x 44pt.
CGFloat kMinimumLength = 44;
}  // namespace

@interface BookmarkExtendedButton ()
@property(nonatomic, assign) BOOL doesExtendEdges;
@end

@implementation BookmarkExtendedButton
@synthesize doesExtendEdges = _doesExtendEdges;
@synthesize extendedEdges = _extendedEdges;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.exclusiveTouch = YES;
  }
  return self;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  return CGRectContainsPoint([self touchArea], point);
}

- (void)setExtendedEdges:(UIEdgeInsets)extendedEdges {
  // Check that the custom edge extensions meet Apple's guidelines.
  DCHECK_GE(
      extendedEdges.top + extendedEdges.bottom + CGRectGetHeight(self.bounds),
      kMinimumLength - 0.001);
  DCHECK_GE(
      extendedEdges.left + extendedEdges.right + CGRectGetWidth(self.bounds),
      kMinimumLength - 0.001);
  _extendedEdges = extendedEdges;
  self.doesExtendEdges = YES;
}

- (CGRect)touchArea {
  UIEdgeInsets reversedInsets;
  if (self.doesExtendEdges) {
    reversedInsets =
        UIEdgeInsetsMake(-self.extendedEdges.top, -self.extendedEdges.left,
                         -self.extendedEdges.bottom, -self.extendedEdges.right);
  } else {
    CGFloat horizontalExtension =
        (kMinimumLength - CGRectGetWidth(self.bounds)) / 2.0;
    horizontalExtension = MAX(horizontalExtension, 0);
    CGFloat verticalExtension =
        (kMinimumLength - CGRectGetHeight(self.bounds)) / 2.0;
    verticalExtension = MAX(verticalExtension, 0);
    reversedInsets = UIEdgeInsetsMake(-verticalExtension, -horizontalExtension,
                                      -verticalExtension, -horizontalExtension);
  }
  return UIEdgeInsetsInsetRect(self.bounds, reversedInsets);
}

- (CGRect)accessibilityFrame {
  CGRect touchArea = [self touchArea];
  CGRect touchAreaInWindowCoordinates = [self convertRect:touchArea toView:nil];
  CGRect touchAreaInScreenCoordinates =
      [self.window convertRect:touchAreaInWindowCoordinates toWindow:nil];
  return touchAreaInScreenCoordinates;
}

@end
