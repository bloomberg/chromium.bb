// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_

#import <Cocoa/Cocoa.h>

namespace info_bubble {

// TODO(andybons): confirm constants with UI dudes.
const CGFloat kBubbleArrowHeight = 8.0;
const CGFloat kBubbleArrowWidth = 15.0;
const CGFloat kBubbleArrowXOffset = 10.0;
const CGFloat kBubbleCornerRadius = 8.0;

enum BubbleArrowLocation {
  kTopLeft,
  kTopRight,
};

enum InfoBubbleType {
  kGradientInfoBubble,
  kWhiteInfoBubble
};

}  // namespace info_bubble

// Content view for a bubble with an arrow showing arbitrary content.
// This is where nonrectangular drawing happens.
@interface InfoBubbleView : NSView {
 @private
  info_bubble::BubbleArrowLocation arrowLocation_;

  // The type simply is used to determine what sort of background it should
  // draw.
  info_bubble::InfoBubbleType bubbleType_;
}

@property (assign, nonatomic) info_bubble::BubbleArrowLocation arrowLocation;
@property (assign, nonatomic) info_bubble::InfoBubbleType bubbleType;

// Returns the point location in view coordinates of the tip of the arrow.
- (NSPoint)arrowTip;

@end

#endif  // CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_
