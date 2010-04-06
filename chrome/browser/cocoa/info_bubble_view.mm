// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/info_bubble_view.h"

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

// TODO(andybons): confirm constants with UI dudes.
extern const CGFloat kBubbleArrowHeight = 8.0;
extern const CGFloat kBubbleArrowWidth = 15.0;
extern const CGFloat kBubbleArrowXOffset = 10.0;
extern const CGFloat kBubbleCornerRadius = 8.0;

@implementation InfoBubbleView

@synthesize arrowLocation = arrowLocation_;
@synthesize bubbleType = bubbleType_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    arrowLocation_ = kTopLeft;
    bubbleType_ = kGradientInfoBubble;
  }

  return self;
}

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.height -= kBubbleArrowHeight;
  NSBezierPath* bezier = [NSBezierPath bezierPath];
  rect.size.height -= kBubbleArrowHeight;

  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds
                                  xRadius:kBubbleCornerRadius
                                  yRadius:kBubbleCornerRadius];

  // Add the bubble arrow.
  CGFloat dX = 0;
  switch (arrowLocation_) {
    case kTopLeft:
      dX = kBubbleArrowXOffset;
      break;
    case kTopRight:
      dX = NSWidth(bounds) - kBubbleArrowXOffset - kBubbleArrowWidth;
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += dX;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth/2.0,
                                  arrowStart.y + kBubbleArrowHeight)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];

  // Then fill the inside depending on the type of bubble.
  if (bubbleType_ == kGradientInfoBubble) {
    NSColor* base_color = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
    NSColor* startColor =
        [base_color gtm_colorAdjustedFor:GTMColorationLightHighlight
                                   faded:YES];
    NSColor* midColor =
        [base_color gtm_colorAdjustedFor:GTMColorationLightMidtone
                                   faded:YES];
    NSColor* endColor =
        [base_color gtm_colorAdjustedFor:GTMColorationLightShadow
                                   faded:YES];
    NSColor* glowColor =
        [base_color gtm_colorAdjustedFor:GTMColorationLightPenumbra
                                   faded:YES];

    scoped_nsobject<NSGradient> gradient(
        [[NSGradient alloc] initWithColorsAndLocations:startColor, 0.0,
                                                       midColor, 0.25,
                                                       endColor, 0.5,
                                                       glowColor, 0.75,
                                                       nil]);

    [gradient.get() drawInBezierPath:bezier angle:0.0];
  } else if (bubbleType_ == kWhiteInfoBubble) {
    [[NSColor whiteColor] set];
    [bezier fill];
  }
}

- (NSPoint)arrowTip {
  NSRect bounds = [self bounds];
  CGFloat tipXOffset = kBubbleArrowXOffset + kBubbleArrowWidth / 2.0;
  CGFloat xOffset = arrowLocation_ == kTopRight ? NSMaxX(bounds) - tipXOffset :
  NSMinX(bounds) + tipXOffset;
  NSPoint arrowTip = NSMakePoint(xOffset, NSMaxY(bounds));
  return arrowTip;
}

@end
