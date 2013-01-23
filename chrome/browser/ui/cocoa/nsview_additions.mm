// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nsview_additions.h"

#include "base/logging.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSView (LionAPI)
- (NSSize)convertSizeFromBacking:(NSSize)size;
@end

#endif  // 10.7

@implementation NSView (ChromeAdditions)

- (CGFloat)cr_lineWidth {
  // All shipping retina macs run at least 10.7.
  if (![self respondsToSelector:@selector(convertSizeFromBacking:)])
    return 1;
  return [self convertSizeFromBacking:NSMakeSize(1, 1)].width;
}

- (BOOL)cr_isMouseInView {
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [[self superview] convertPoint:mouseLoc fromView:nil];
  return [self hitTest:mouseLoc] == self;
}

- (BOOL)cr_isBelowView:(NSView*)otherView {
  NSArray* subviews = [[self superview] subviews];

  NSUInteger selfIndex = [subviews indexOfObject:self];
  DCHECK_NE(NSNotFound, selfIndex);

  NSUInteger otherIndex = [subviews indexOfObject:otherView];
  DCHECK_NE(NSNotFound, otherIndex);

  return selfIndex < otherIndex;
}

- (BOOL)cr_isAboveView:(NSView*)otherView {
  return ![self cr_isBelowView:otherView];
}

- (void)cr_ensureSubview:(NSView*)subview
            isPositioned:(NSWindowOrderingMode)place
              relativeTo:(NSView *)otherView {
  DCHECK(place == NSWindowAbove || place == NSWindowBelow);
  BOOL isAbove = place == NSWindowAbove;
  if ([[subview superview] isEqual:self] &&
      [subview cr_isAboveView:otherView] == isAbove) {
    return;
  }

  [subview removeFromSuperview];
  [self addSubview:subview
        positioned:place
        relativeTo:otherView];
}

@end
