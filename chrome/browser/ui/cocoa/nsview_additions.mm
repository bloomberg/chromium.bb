// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nsview_additions.h"

@implementation NSView (ChromeAdditions)

- (CGFloat)cr_lineWidth {
  return 1.0 / [self convertSizeToBase:NSMakeSize(1, 1)].width;
}

- (BOOL)cr_isMouseInView {
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [[self superview] convertPoint:mouseLoc fromView:nil];
  return [self hitTest:mouseLoc] == self;
}

@end
