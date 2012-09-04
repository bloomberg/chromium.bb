// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nsview_additions.h"

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

@end
