// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/info_bubble_view.h"

#include "base/logging.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSBezierPath+RoundRect.h"

@implementation InfoBubbleView

@synthesize arrowLocation = arrowLocation_;
@synthesize alignment = alignment_;
@synthesize cornerFlags = cornerFlags_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    arrowLocation_ = info_bubble::kTopLeft;
    alignment_ = info_bubble::kAlignArrowToAnchor;
    cornerFlags_ = info_bubble::kRoundedAllCorners;
    backgroundColor_.reset([[NSColor whiteColor] retain]);
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  if (arrowLocation_ != info_bubble::kNoArrow) {
    bounds.size.height -= info_bubble::kBubbleArrowHeight;
  }
  rect.size.height -= info_bubble::kBubbleArrowHeight;

  float topRadius = cornerFlags_ & info_bubble::kRoundedTopCorners ?
      info_bubble::kBubbleCornerRadius : 0;
  float bottomRadius = cornerFlags_ & info_bubble::kRoundedBottomCorners ?
      info_bubble::kBubbleCornerRadius : 0;

  NSBezierPath* bezier =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                            topLeftCornerRadius:topRadius
                           topRightCornerRadius:topRadius
                         bottomLeftCornerRadius:bottomRadius
                        bottomRightCornerRadius:bottomRadius];

  // Add the bubble arrow.
  CGFloat dX = 0;
  switch (arrowLocation_) {
    case info_bubble::kTopLeft:
      dX = info_bubble::kBubbleArrowXOffset;
      break;
    case info_bubble::kTopRight:
      dX = NSWidth(bounds) - info_bubble::kBubbleArrowXOffset -
          info_bubble::kBubbleArrowWidth;
      break;
    case info_bubble::kTopCenter:
      dX = NSMidX(bounds) - info_bubble::kBubbleArrowWidth / 2.0;
      break;
    case info_bubble::kNoArrow:
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += dX;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  if (arrowLocation_ != info_bubble::kNoArrow) {
    [bezier lineToPoint:NSMakePoint(arrowStart.x +
                                        info_bubble::kBubbleArrowWidth / 2.0,
                                    arrowStart.y +
                                        info_bubble::kBubbleArrowHeight)];
  }
  [bezier lineToPoint:NSMakePoint(arrowStart.x + info_bubble::kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];
  [backgroundColor_ set];
  [bezier fill];
}

- (NSPoint)arrowTip {
  NSRect bounds = [self bounds];
  CGFloat tipXOffset =
      info_bubble::kBubbleArrowXOffset + info_bubble::kBubbleArrowWidth / 2.0;
  CGFloat xOffset = 0.0;
  switch(arrowLocation_) {
    case info_bubble::kTopRight:
      xOffset = NSMaxX(bounds) - tipXOffset;
      break;
    case info_bubble::kTopLeft:
      xOffset = NSMinX(bounds) + tipXOffset;
      break;
    case info_bubble::kTopCenter:
      xOffset = NSMidX(bounds);
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowTip = NSMakePoint(xOffset, NSMaxY(bounds));
  return arrowTip;
}

- (NSColor*)backgroundColor {
  return backgroundColor_;
}

- (void)setBackgroundColor:(NSColor*)backgroundColor {
  backgroundColor_.reset([backgroundColor retain]);
}

@end
