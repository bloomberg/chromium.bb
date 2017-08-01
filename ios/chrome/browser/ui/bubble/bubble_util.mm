// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace bubble_util {

// Calculate the coordinates of the point of the speech bubble's triangle based
// on the |frame| of the target UI element and the bubble's |arrowDirection|.
// The returned point is in the same coordinate system as |frame|.
CGPoint AnchorPoint(CGRect targetFrame, BubbleArrowDirection arrowDirection) {
  CGPoint anchorPoint;
  anchorPoint.x = CGRectGetMidX(targetFrame);
  if (arrowDirection == BubbleArrowDirectionUp) {
    anchorPoint.y = CGRectGetMaxY(targetFrame);
  } else {
    DCHECK_EQ(arrowDirection, BubbleArrowDirectionDown);
    anchorPoint.y = CGRectGetMinY(targetFrame);
  }
  return anchorPoint;
}

CGFloat LeadingDistance(CGRect targetFrame,
                        BubbleAlignment alignment,
                        CGFloat bubbleWidth) {
  // Find |leadingOffset|, the distance from the bubble's leading edge to the
  // anchor point. This depends on alignment and bubble width.
  CGFloat anchorX = CGRectGetMidX(targetFrame);
  CGFloat leadingOffset;
  switch (alignment) {
    case BubbleAlignmentLeading:
      leadingOffset = kBubbleAlignmentOffset;
      break;
    case BubbleAlignmentCenter:
      leadingOffset = bubbleWidth / 2.0f;
      break;
    case BubbleAlignmentTrailing:
      leadingOffset = bubbleWidth - kBubbleAlignmentOffset;
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  return anchorX - leadingOffset;
}

CGFloat OriginY(CGRect targetFrame,
                BubbleArrowDirection arrowDirection,
                CGFloat bubbleHeight) {
  CGPoint anchorPoint = AnchorPoint(targetFrame, arrowDirection);
  CGFloat originY;
  if (arrowDirection == BubbleArrowDirectionUp) {
    originY = anchorPoint.y;
  } else {
    DCHECK_EQ(arrowDirection, BubbleArrowDirectionDown);
    originY = anchorPoint.y - bubbleHeight;
  }
  return originY;
}

CGFloat MaxWidth(CGRect targetFrame,
                 BubbleAlignment alignment,
                 CGFloat boundingWidth,
                 bool isRTL) {
  CGFloat anchorX = CGRectGetMidX(targetFrame);
  CGFloat maxWidth;
  switch (alignment) {
    case BubbleAlignmentLeading:
      if (isRTL) {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within |kBubbleAlignmentOffset| from the right.
        maxWidth = anchorX + kBubbleAlignmentOffset;
      } else {
        // The bubble is aligned left, and can use space to the right of the
        // anchor point and within |kBubbleAlignmentOffset| from the left.
        maxWidth = boundingWidth - anchorX + kBubbleAlignmentOffset;
      }
      break;
    case BubbleAlignmentCenter:
      // The width of half the bubble cannot exceed the distance from the anchor
      // point to the closest edge of the superview.
      maxWidth = MIN(anchorX, boundingWidth - anchorX) * 2.0f;
      break;
    case BubbleAlignmentTrailing:
      if (isRTL) {
        // The bubble is aligned left, and can use space to the right of the
        // anchor point and within |kBubbleAlignmentOffset| from the left.
        maxWidth = boundingWidth - anchorX + kBubbleAlignmentOffset;
      } else {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within |kBubbleAlignmentOffset| from the right.
        maxWidth = anchorX + kBubbleAlignmentOffset;
      }
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  return maxWidth;
}

CGFloat MaxWidth(CGRect targetFrame,
                 BubbleAlignment alignment,
                 CGFloat boundingWidth) {
  bool isRTL = base::i18n::IsRTL();
  return MaxWidth(targetFrame, alignment, boundingWidth, isRTL);
}

}  // namespace bubble_util
