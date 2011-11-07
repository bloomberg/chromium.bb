// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_INFO_BUBBLE_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>

namespace info_bubble {

const CGFloat kBubbleArrowHeight = 8.0;
const CGFloat kBubbleArrowWidth = 15.0;
const CGFloat kBubbleCornerRadius = 8.0;
const CGFloat kBubbleArrowXOffset = kBubbleArrowWidth + kBubbleCornerRadius;

enum BubbleArrowLocation {
  kTopLeft,
  kTopRight,
  kNoArrow,
};

enum BubbleAlignment {
  // The tip of the arrow points to the anchor point.
  kAlignArrowToAnchor,
  // The edge nearest to the arrow is lined up with the anchor point.
  kAlignEdgeToAnchorEdge,
};

}  // namespace info_bubble

// Content view for a bubble with an arrow showing arbitrary content.
// This is where nonrectangular drawing happens.
@interface InfoBubbleView : NSView {
 @private
  info_bubble::BubbleArrowLocation arrowLocation_;
  info_bubble::BubbleAlignment alignment_;
}

@property(assign, nonatomic) info_bubble::BubbleArrowLocation arrowLocation;
@property(assign, nonatomic) info_bubble::BubbleAlignment alignment;

// Returns the point location in view coordinates of the tip of the arrow.
- (NSPoint)arrowTip;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFO_BUBBLE_VIEW_H_
