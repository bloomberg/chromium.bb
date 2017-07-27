// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

namespace bubble_util {

// Calculate the distance from the bubble's leading edge to the leading edge of
// its bounding coordinate system. In LTR contexts, the returned float is the
// x-coordinate of the bubble's origin. This calculation is based on
// |targetFrame|, which is the frame of the target UI element, and the bubble's
// alignment, direction, and size. The returned float is in the same coordinate
// system as |targetFrame|, which should be the coordinate system in which the
// bubble is drawn.
CGFloat LeadingDistance(CGRect targetFrame,
                        BubbleAlignment alignment,
                        CGFloat bubbleWidth);

// Calculate the y-coordinate of the bubble's origin based on |targetFrame|, and
// the bubble's arrow direction and size. The returned float is in the same
// coordinate system as |targetFrame|, which should be the coordinate system in
// which the bubble is drawn.
CGFloat OriginY(CGRect targetFrame,
                BubbleArrowDirection arrowDirection,
                CGFloat bubbleHeight);

// Calculate the maximum width of the bubble such that it stays within its
// bounding coordinate space. |targetFrame| is the frame the target UI element
// in the coordinate system in which the bubble is drawn. |alignment| is the
// bubble's alignment, and |boundingWidth| is the width of the coordinate space
// in which the bubble is drawn.
CGFloat MaxWidth(CGRect targetFrame,
                 BubbleAlignment alignment,
                 CGFloat boundingWidth);

}  // namespace bubble_util

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_
