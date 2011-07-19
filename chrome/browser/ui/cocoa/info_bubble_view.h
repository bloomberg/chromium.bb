// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
};

}  // namespace info_bubble

// Content view for a bubble with an arrow showing arbitrary content.
// This is where nonrectangular drawing happens.
@interface InfoBubbleView : NSView {
 @private
  info_bubble::BubbleArrowLocation arrowLocation_;
}

@property(assign, nonatomic) info_bubble::BubbleArrowLocation arrowLocation;

// Returns the point location in view coordinates of the tip of the arrow.
- (NSPoint)arrowTip;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFO_BUBBLE_VIEW_H_
