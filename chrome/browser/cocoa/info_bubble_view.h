// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_

#import <Cocoa/Cocoa.h>

extern const CGFloat kBubbleArrowHeight;
extern const CGFloat kBubbleArrowWidth;
extern const CGFloat kBubbleArrowXOffset;
extern const CGFloat kBubbleCornerRadius;

enum BubbleArrowLocation {
  kTopLeft,
  kTopRight,
};

enum InfoBubbleType {
  kGradientInfoBubble,
  kWhiteInfoBubble
};

// Content view for a bubble with an arrow showing arbitrary content.
// This is where nonrectangular drawing happens.
@interface InfoBubbleView : NSView {
 @private
  BubbleArrowLocation arrowLocation_;

  // The type simply is used to determine what sort of background it should
  // draw.
  InfoBubbleType bubbleType_;
}

@property (assign, nonatomic) BubbleArrowLocation arrowLocation;
@property (assign, nonatomic) InfoBubbleType bubbleType;

@end

#endif  // CHROME_BROWSER_COCOA_INFO_BUBBLE_VIEW_H_
