// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/chevron_menu_button_cell.h"

namespace {

// Width of the divider.
const CGFloat kDividerWidth = 1.0;

// Vertical inset from edge of cell to divider start.
const CGFloat kDividerInset = 3.0;

// Grayscale for the center of the divider.
const CGFloat kDividerGrayscale = 0.5;

}  // namespace

@implementation ChevronMenuButtonCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [super drawWithFrame:cellFrame inView:controlView];

  if ([self isMouseInside])
    return;

  NSColor* middleColor =
      [NSColor colorWithCalibratedWhite:kDividerGrayscale alpha:1.0];
  NSColor* endPointColor = [middleColor colorWithAlphaComponent:0.0];

  // Blend from background to |kDividerGrayscale| and back to
  // background.
  scoped_nsobject<NSGradient> borderGradient([[NSGradient alloc]
      initWithColorsAndLocations:endPointColor, (CGFloat)0.0,
                                 middleColor, (CGFloat)0.5,
                                 endPointColor, (CGFloat)1.0,
                                 nil]);

  NSRect edgeRect, remainder;
  NSDivideRect(cellFrame, &edgeRect, &remainder, kDividerWidth, NSMaxXEdge);
  edgeRect = NSInsetRect(edgeRect, 0.0, kDividerInset);

  [borderGradient drawInRect:edgeRect angle:90.0];
}

@end
