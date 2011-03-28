// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/side_tab_strip_view.h"

#include "base/memory/scoped_nsobject.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

@implementation SideTabStripView

- (void)drawBorder:(NSRect)bounds {
  // Draw a border on the right side.
  NSRect borderRect, contentRect;
  NSDivideRect(bounds, &borderRect, &contentRect, 1, NSMaxXEdge);
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

// Override to prevent double-clicks from minimizing the window. The side
// tab strip doesn't have that behavior (since it's in the window content
// area).
- (BOOL)doubleClickMinimizesWindow {
  return NO;
}

- (void)drawRect:(NSRect)rect {
  // BOOL isKey = [[self window] isKeyWindow];
  NSColor* aColor =
      [NSColor colorWithCalibratedRed:0.506 green:0.660 blue:0.985 alpha:1.000];
  NSColor* bColor =
      [NSColor colorWithCalibratedRed:0.099 green:0.140 blue:0.254 alpha:1.000];
  scoped_nsobject<NSGradient> gradient(
      [[NSGradient alloc] initWithStartingColor:aColor endingColor:bColor]);

  NSRect gradientRect = [self bounds];
  [gradient drawInRect:gradientRect angle:270.0];

  // Draw borders and any drop feedback.
  [super drawRect:rect];
}

@end
