// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/rtl_geometry.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/logging.h"

bool UseRTLLayout() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater();
}

base::i18n::TextDirection LayoutDirection() {
  return UseRTLLayout() ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
}

#pragma mark - LayoutRects.

const LayoutRect LayoutRectZero = {0.0, 0.0, 0.0, CGSizeMake(0.0, 0.0)};

LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height) {
  LayoutRect layout = {leading, boundingWidth, originY,
                       CGSizeMake(width, height)};
  return layout;
}

CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction) {
  CGRect rect;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    CGFloat trailing =
        layout.boundingWidth - (layout.leading + layout.size.width);
    rect = CGRectMake(trailing, layout.originY, layout.size.width,
                      layout.size.height);
  } else {
    DCHECK_EQ(direction, base::i18n::LEFT_TO_RIGHT);
    rect = CGRectMake(layout.leading, layout.originY, layout.size.width,
                      layout.size.height);
  }
  return rect;
}

CGRect LayoutRectGetRect(LayoutRect layout) {
  return LayoutRectGetRectUsingDirection(layout, LayoutDirection());
}

CGRect LayoutRectGetBoundsRect(LayoutRect layout) {
  return CGRectMake(0, 0, layout.size.width, layout.size.height);
}

CGPoint LayoutRectGetPositionForAnchorUsingDirection(
    LayoutRect layout,
    CGPoint anchor,
    base::i18n::TextDirection direction) {
  CGRect rect = LayoutRectGetRectUsingDirection(layout, direction);
  return CGPointMake(CGRectGetMinX(rect) + (rect.size.width * anchor.x),
                     CGRectGetMinY(rect) + (rect.size.height * anchor.y));
}

CGPoint LayoutRectGetPositionForAnchor(LayoutRect layout, CGPoint anchor) {
  return LayoutRectGetPositionForAnchorUsingDirection(layout, anchor,
                                                      LayoutDirection());
}

LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect contextRect,
    base::i18n::TextDirection direction) {
  LayoutRect layout;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    layout.leading = contextRect.size.width - CGRectGetMaxX(rect);
  } else {
    layout.leading = CGRectGetMinX(rect);
  }
  layout.boundingWidth = contextRect.size.width;
  layout.originY = rect.origin.y;
  layout.size = rect.size;
  return layout;
}

LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect contextRect) {
  return LayoutRectForRectInBoundingRectUsingDirection(rect, contextRect,
                                                       LayoutDirection());
}

LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  leadingLayout.leading = 0;
  leadingLayout.boundingWidth = layout.boundingWidth;
  leadingLayout.originY = leadingLayout.originY;
  leadingLayout.size = CGSizeMake(layout.leading, layout.size.height);
  return leadingLayout;
}

LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  CGFloat trailing = LayoutRectGetTrailingEdge(layout);
  leadingLayout.leading = trailing;
  leadingLayout.boundingWidth = layout.boundingWidth;
  leadingLayout.originY = leadingLayout.originY;
  leadingLayout.size =
      CGSizeMake((layout.boundingWidth - trailing), layout.size.height);
  return leadingLayout;
}

CGFloat LayoutRectGetTrailingEdge(LayoutRect layout) {
  return layout.leading + layout.size.width;
}

#pragma mark - UIKit utilities

UIViewAutoresizing UIViewAutoresizingFlexibleLeadingMargin() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater()
             ? UIViewAutoresizingFlexibleRightMargin
             : UIViewAutoresizingFlexibleLeftMargin;
}

UIViewAutoresizing UIViewAutoresizingFlexibleTrailingMargin() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater()
             ? UIViewAutoresizingFlexibleLeftMargin
             : UIViewAutoresizingFlexibleRightMargin;
}

UIEdgeInsets UIEdgeInsetsMakeUsingDirection(
    CGFloat top,
    CGFloat leading,
    CGFloat bottom,
    CGFloat trailing,
    base::i18n::TextDirection direction) {
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    using std::swap;
    swap(leading, trailing);
  }
  // At this point, |leading| == left, |trailing| = right.
  return UIEdgeInsetsMake(top, leading, bottom, trailing);
}

UIEdgeInsets UIEdgeInsetsMakeDirected(CGFloat top,
                                      CGFloat leading,
                                      CGFloat bottom,
                                      CGFloat trailing) {
  return UIEdgeInsetsMakeUsingDirection(top, leading, bottom, trailing,
                                        LayoutDirection());
}

#pragma mark - autolayout utilities

NSLayoutFormatOptions LayoutOptionForRTLSupport() {
  if (UseRTLLayout()) {
    return NSLayoutFormatDirectionLeadingToTrailing;
  }
  return NSLayoutFormatDirectionLeftToRight;
}

#pragma mark - deprecated functions

bool IsRTLUILayout() {
  if (base::ios::IsRunningOnIOS9OrLater()) {
#if __IPHONE_9_0
    // Calling this method is better than using the locale on iOS9 since it will
    // work with the right to left pseudolanguage.
    return [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                       UISemanticContentAttributeUnspecified] ==
           UIUserInterfaceLayoutDirectionRightToLeft;
#endif
  }
  // Using NSLocale instead of base::i18n::IsRTL() in order to take into account
  // right to left pseudolanguage correctly (which base::i18n::IsRTL() doesn't).
  return [NSLocale characterDirectionForLanguage:
                       [[NSLocale currentLocale]
                           objectForKey:NSLocaleLanguageCode]] ==
         NSLocaleLanguageDirectionRightToLeft;
}
