// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_FANCY_UI_BIDI_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FANCY_UI_BIDI_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"

namespace ios {
namespace rtl {
enum AdjustSubviewsForRTLType {
  ADJUST_FRAME_AND_AUTOROTATION_MASK_FOR_ALL_SUBVIEWS,
  ADJUST_FRAME_FOR_UPDATED_SUBVIEWS
};
}  // namespace rtl
}  // namespace ios

// A UIView that lays outs its subviews based on the direction of the current
// language. If the current language is RTL then during the first call to
// |layoutSubviews|, this view will flip horizontally all its subviews around
// its center axis and adjust the autoresizing masks.
@interface BidiContainerView : UIView {
 @private
  ios::rtl::AdjustSubviewsForRTLType adjustSubviewsType_;
  base::scoped_nsobject<NSMutableSet> subviewsToBeAdjustedForRTL_;
}

// Adds |subview| to the set of subviews that need to be adjusted to RTL. If the
// current language is RTL, then the next call |layoutSubviews| will flip
// |subview| horizontally around its center axis.
- (void)setSubviewNeedsAdjustmentForRTL:(UIView*)subview;

@end

#endif  // IOS_CHROME_BROWSER_UI_FANCY_UI_BIDI_CONTAINER_VIEW_H_
