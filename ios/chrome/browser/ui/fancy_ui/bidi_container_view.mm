// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fancy_ui/bidi_container_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace rtl {
enum AdjustSubviewsForRTLType {
  ADJUST_FRAME_AND_AUTOROTATION_MASK_FOR_ALL_SUBVIEWS,
  ADJUST_FRAME_FOR_UPDATED_SUBVIEWS
};
}  // namespace rtl
}  // namespace ios

@interface BidiContainerView () {
  ios::rtl::AdjustSubviewsForRTLType adjustSubviewsType_;
  NSMutableSet* subviewsToBeAdjustedForRTL_;
}
// Changes the autoresizing mask by mirroring it horizontally so that the RTL
// layout is bind to opposite sites than LRT one.
+ (UIViewAutoresizing)mirrorAutoresizingMask:
    (UIViewAutoresizing)autoresizingMask;
@end

@implementation BidiContainerView

+ (UIViewAutoresizing)mirrorAutoresizingMask:
    (UIViewAutoresizing)autoresizingMask {
  UIViewAutoresizing mirroredAutoresizingMask = autoresizingMask;
  mirroredAutoresizingMask &= ~(UIViewAutoresizingFlexibleRightMargin |
                                UIViewAutoresizingFlexibleLeftMargin);

  if (autoresizingMask & UIViewAutoresizingFlexibleRightMargin)
    mirroredAutoresizingMask |= UIViewAutoresizingFlexibleLeftMargin;
  if (autoresizingMask & UIViewAutoresizingFlexibleLeftMargin)
    mirroredAutoresizingMask |= UIViewAutoresizingFlexibleRightMargin;

  return mirroredAutoresizingMask;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (!base::i18n::IsRTL())
    return;

  NSArray* viewsToAdjust;
  BOOL adjustAutoresizingMask;
  switch (adjustSubviewsType_) {
    case ios::rtl::ADJUST_FRAME_AND_AUTOROTATION_MASK_FOR_ALL_SUBVIEWS:
      viewsToAdjust = self.subviews;
      adjustAutoresizingMask = YES;
      adjustSubviewsType_ = ios::rtl::ADJUST_FRAME_FOR_UPDATED_SUBVIEWS;
      break;

    case ios::rtl::ADJUST_FRAME_FOR_UPDATED_SUBVIEWS:
      viewsToAdjust = [subviewsToBeAdjustedForRTL_ allObjects];
      adjustAutoresizingMask = NO;
      break;
  }
  if (![viewsToAdjust count])
    return;

  CGFloat frameWidth = self.frame.size.width;
  for (UIView* subview in viewsToAdjust) {
    CGRect subFrame = subview.frame;
    subFrame.origin.x = frameWidth - subFrame.origin.x - subFrame.size.width;
    subview.frame = subFrame;
    if (adjustAutoresizingMask) {
      UIViewAutoresizing mirroredAutoresizingMask =
          [BidiContainerView mirrorAutoresizingMask:[subview autoresizingMask]];
      [subview setAutoresizingMask:mirroredAutoresizingMask];
    }
  }
  [subviewsToBeAdjustedForRTL_ removeAllObjects];
}

- (void)setSubviewNeedsAdjustmentForRTL:(UIView*)subview {
  DCHECK([self.subviews containsObject:subview]);
  if (!base::i18n::IsRTL())
    return;
  if (!subviewsToBeAdjustedForRTL_)
    subviewsToBeAdjustedForRTL_ = [[NSMutableSet alloc] init];
  [subviewsToBeAdjustedForRTL_ addObject:subview];
}

@end
