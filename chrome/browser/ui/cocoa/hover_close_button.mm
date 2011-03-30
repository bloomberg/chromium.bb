// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_close_button.h"

#include "base/memory/scoped_nsobject.h"
#include "grit/generated_resources.h"
#import "third_party/molokocacao/NSBezierPath+MCAdditions.h"
#include "ui/base/l10n/l10n_util.h"

namespace  {
const CGFloat kCircleRadius = 0.415 * 16;
const CGFloat kCircleHoverWhite = 0.565;
const CGFloat kCircleClickWhite = 0.396;
const CGFloat kXShadowAlpha = 0.75;
const CGFloat kXShadowCircleAlpha = 0.1;
}  // namespace

@interface HoverCloseButton(Private)
- (void)updatePaths;
- (void)setUpDrawingPaths;
@end

@implementation HoverCloseButton

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self commonInit];
}

- (void)drawRect:(NSRect)rect {
  if (!circlePath_.get() || !xPath_.get())
    [self setUpDrawingPaths];

  // Only call updatePaths if the size changed.
  if (!NSEqualSizes(oldSize_, [self bounds].size))
    [self updatePaths];

  // If the user is hovering over the button, a light/dark gray circle is drawn
  // behind the 'x'.
  if (hoverState_ != kHoverStateNone) {
    // Adjust the darkness of the circle depending on whether it is being
    // clicked.
    CGFloat white = (hoverState_ == kHoverStateMouseOver) ?
        kCircleHoverWhite : kCircleClickWhite;
    [[NSColor colorWithCalibratedWhite:white alpha:1.0] set];
    [circlePath_ fill];
  }

  [[NSColor whiteColor] set];
  [xPath_ fill];

  // Give the 'x' an inner shadow for depth. If the button is in a hover state
  // (circle behind it), then adjust the shadow accordingly (not as harsh).
  NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
  CGFloat alpha = (hoverState_ != kHoverStateNone) ?
      kXShadowCircleAlpha : kXShadowAlpha;
  [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.15
                                                     alpha:alpha]];
  [shadow setShadowOffset:NSMakeSize(0.0, 0.0)];
  [shadow setShadowBlurRadius:2.5];
  [xPath_ fillWithInnerShadow:shadow];
}

- (void)commonInit {
  // Set accessibility description.
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_CLOSE);
  [[self cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];

  // Add a tooltip.
  [self setToolTip:l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_CLOSE_TAB)];
}

- (void)setUpDrawingPaths {
  // Keep the paths centered around the origin in this function. It is then
  // translated in -updatePaths.
  NSPoint xCenter = NSZeroPoint;

  circlePath_.reset([[NSBezierPath bezierPath] retain]);
  [circlePath_ moveToPoint:xCenter];
  CGFloat radius = kCircleRadius;
  [circlePath_ appendBezierPathWithArcWithCenter:xCenter
                                          radius:radius
                                      startAngle:0.0
                                        endAngle:365.0];

  // Construct an 'x' by drawing two intersecting rectangles in the shape of a
  // cross and then rotating the path by 45 degrees.
  xPath_.reset([[NSBezierPath bezierPath] retain]);
  [xPath_ appendBezierPathWithRect:NSMakeRect(3.5, 7.0, 9.0, 2.0)];
  [xPath_ appendBezierPathWithRect:NSMakeRect(7.0, 3.5, 2.0, 9.0)];

  NSRect pathBounds = [xPath_ bounds];
  NSPoint pathCenter = NSMakePoint(NSMidX(pathBounds), NSMidY(pathBounds));

  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform translateXBy:xCenter.x yBy:xCenter.y];
  [transform rotateByDegrees:45.0];
  [transform translateXBy:-pathCenter.x yBy:-pathCenter.y];

  [xPath_ transformUsingAffineTransform:transform];
}

- (void)updatePaths {
  oldSize_ = [self bounds].size;

  // Revert the current transform for the two points.
  if (transform_.get()) {
    [transform_.get() invert];
    [circlePath_.get() transformUsingAffineTransform:transform_.get()];
    [xPath_.get() transformUsingAffineTransform:transform_.get()];
  }

  // Create the new transform. [self bounds] is prefered in case aRect wasn't
  // literally taken as bounds (e.g. cropped).
  NSPoint xCenter = NSMakePoint(8, oldSize_.height / 2.0f);

  // Retain here, as scoped_* don't retain.
  transform_.reset([[NSAffineTransform transform] retain]);
  [transform_.get() translateXBy:xCenter.x yBy:xCenter.y];
  [circlePath_.get() transformUsingAffineTransform:transform_.get()];
  [xPath_.get() transformUsingAffineTransform:transform_.get()];
}

@end
