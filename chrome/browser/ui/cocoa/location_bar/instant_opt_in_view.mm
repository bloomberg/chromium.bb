// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/location_bar/instant_opt_in_view.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"

namespace {
// How to round off the popup's corners.  Goal is to match star and go
// buttons.
const CGFloat kPopupRoundingRadius = 3.5;

// How far from the top of the view to place the horizontal line.
const CGFloat kHorizontalLineTopOffset = 2;

// How far from the sides to inset the horizontal line.
const CGFloat kHorizontalLineInset = 2;
}

@implementation InstantOptInView

- (void)drawRect:(NSRect)rect {
  // Round off the bottom corners only.
  NSBezierPath* path =
     [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                           topLeftCornerRadius:0
                          topRightCornerRadius:0
                        bottomLeftCornerRadius:kPopupRoundingRadius
                       bottomRightCornerRadius:kPopupRoundingRadius];

  [NSGraphicsContext saveGraphicsState];
  [path addClip];

  // Background is white.
  [[NSColor whiteColor] set];
  NSRectFill(rect);

  // Draw a horizontal line 2 px down from the top of the view, inset at the
  // sides by 2 px.
  CGFloat lineY = NSMaxY([self bounds]) - kHorizontalLineTopOffset;
  CGFloat minX = std::min(NSMinX([self bounds]) + kHorizontalLineInset,
                          NSMaxX([self bounds]));
  CGFloat maxX = std::max(NSMaxX([self bounds]) - kHorizontalLineInset,
                          NSMinX([self bounds]));

  [[NSColor lightGrayColor] set];
  NSRectFill(NSMakeRect(minX, lineY, maxX - minX, 1));

  [NSGraphicsContext restoreGraphicsState];
}

@end
