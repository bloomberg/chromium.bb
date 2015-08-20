// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_util.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/gfx/ios/uikit_util.h"

bool IsIPadIdiom() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return idiom == UIUserInterfaceIdiomPad;
}

const CGFloat kPortraitWidth[INTERFACE_IDIOM_COUNT] = {
    320,  // IPHONE_IDIOM
    768   // IPAD_IDIOM
};

bool UseRTLLayout() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater();
}

base::i18n::TextDirection LayoutDirection() {
  return UseRTLLayout() ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
}

CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction) {
  CGRect rect;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    CGFloat trailing =
        layout.contextWidth - (layout.leading + layout.size.width);
    rect = CGRectMake(trailing, layout.yOrigin, layout.size.width,
                      layout.size.height);
  } else {
    DCHECK_EQ(direction, base::i18n::LEFT_TO_RIGHT);
    rect = CGRectMake(layout.leading, layout.yOrigin, layout.size.width,
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

LayoutRect LayoutRectForRectInRectUsingDirection(
    CGRect rect,
    CGRect contextRect,
    base::i18n::TextDirection direction) {
  LayoutRect layout;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    layout.leading = contextRect.size.width - CGRectGetMaxX(rect);
  } else {
    layout.leading = CGRectGetMinX(rect);
  }
  layout.contextWidth = contextRect.size.width;
  layout.yOrigin = rect.origin.y;
  layout.size = rect.size;
  return layout;
}

LayoutRect LayoutRectForRectInRect(CGRect rect, CGRect contextRect) {
  return LayoutRectForRectInRectUsingDirection(rect, contextRect,
                                               LayoutDirection());
}

LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  leadingLayout.leading = 0;
  leadingLayout.contextWidth = layout.contextWidth;
  leadingLayout.yOrigin = leadingLayout.yOrigin;
  leadingLayout.size = CGSizeMake(layout.leading, layout.size.height);
  return leadingLayout;
}

LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  CGFloat trailing = LayoutRectGetTrailing(layout);
  leadingLayout.leading = trailing;
  leadingLayout.contextWidth = layout.contextWidth;
  leadingLayout.yOrigin = leadingLayout.yOrigin;
  leadingLayout.size =
      CGSizeMake((layout.contextWidth - trailing), layout.size.height);
  return leadingLayout;
}

CGFloat LayoutRectGetTrailing(LayoutRect layout) {
  return layout.leading + layout.size.width;
}

bool IsHighResScreen() {
  return [[UIScreen mainScreen] scale] > 1.0;
}

bool IsPortrait() {
  UIInterfaceOrientation orient = GetInterfaceOrientation();
// If building with an SDK prior to iOS 8 don't worry about
// UIInterfaceOrientationUnknown because it wasn't defined.
#if !defined(__IPHONE_8_0) || __IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_8_0
  return UIInterfaceOrientationIsPortrait(orient);
#else
  return UIInterfaceOrientationIsPortrait(orient) ||
         orient == UIInterfaceOrientationUnknown;
#endif  // SDK
}

bool IsLandscape() {
  return UIInterfaceOrientationIsLandscape(GetInterfaceOrientation());
}

CGFloat CurrentScreenHeight() {
  CGSize screenSize = [UIScreen mainScreen].bounds.size;
  if (base::ios::IsRunningOnIOS8OrLater()) {
    return screenSize.height;
  } else {
    return IsPortrait() ? screenSize.height : screenSize.width;
  }
}

CGFloat CurrentScreenWidth() {
  CGSize screenSize = [UIScreen mainScreen].bounds.size;
  if (base::ios::IsRunningOnIOS8OrLater()) {
    return screenSize.width;
  } else {
    return IsPortrait() ? screenSize.width : screenSize.height;
  }
}

CGFloat StatusBarHeight() {
  // TODO(justincohen): This is likely not correct, but right now iOS7 betas are
  // doing strange things when handling calls that change the status bar height.
  // For now, just return 20. crbug/264367
  return 20;
}

CGFloat AlignValueToPixel(CGFloat value) {
  static CGFloat scale = [[UIScreen mainScreen] scale];
  return floor(value * scale) / scale;
}

CGPoint AlignPointToPixel(CGPoint point) {
  return CGPointMake(AlignValueToPixel(point.x), AlignValueToPixel(point.y));
}

CGRect AlignRectToPixel(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  return rect;
}

CGRect AlignRectOriginAndSizeToPixels(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  rect.size = ui::AlignSizeToUpperPixel(rect.size);
  return rect;
}

CGRect CGRectCopyWithOrigin(CGRect rect, CGFloat x, CGFloat y) {
  return CGRectMake(x, y, rect.size.width, rect.size.height);
}

CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width) {
  return AlignRectOriginAndSizeToPixels(
      CGRectMake(x - width / 2.0, y - width / 2.0, width, width));
}

// Based on an original size and a target size applies the transformations.
void CalculateProjection(CGSize originalSize,
                         CGSize desiredTargetSize,
                         ProjectionMode projectionMode,
                         CGSize& targetSize,
                         CGRect& projectTo) {
  targetSize = desiredTargetSize;
  projectTo = CGRectZero;
  if (originalSize.height < 1 || originalSize.width < 1)
    return;
  if (targetSize.height < 1 || targetSize.width < 1)
    return;

  CGFloat aspectRatio = originalSize.width / originalSize.height;
  CGFloat targetAspectRatio = targetSize.width / targetSize.height;
  switch (projectionMode) {
    case ProjectionMode::kFill:
      // Don't preserve the aspect ratio.
      projectTo.size = targetSize;
      break;

    case ProjectionMode::kAspectFill:
      if (targetAspectRatio < aspectRatio) {
        // Clip the x-axis.
        projectTo.size.width = targetSize.height * aspectRatio;
        projectTo.size.height = targetSize.height;
        projectTo.origin.x = (targetSize.width - projectTo.size.width) / 2;
        projectTo.origin.y = 0;
      } else {
        // Clip the y-axis.
        projectTo.size.width = targetSize.width;
        projectTo.size.height = targetSize.width / aspectRatio;
        projectTo.origin.x = 0;
        projectTo.origin.y = (targetSize.height - projectTo.size.height) / 2;
      }
      break;

    case ProjectionMode::kAspectFit:
      if (targetAspectRatio < aspectRatio) {
        projectTo.size.width = targetSize.width;
        projectTo.size.height = projectTo.size.width / aspectRatio;
        targetSize = projectTo.size;
      } else {
        projectTo.size.height = targetSize.height;
        projectTo.size.width = projectTo.size.height * aspectRatio;
        targetSize = projectTo.size;
      }
      break;

    case ProjectionMode::kAspectFillNoClipping:
      if (targetAspectRatio < aspectRatio) {
        targetSize.width = targetSize.height * aspectRatio;
        targetSize.height = targetSize.height;
      } else {
        targetSize.width = targetSize.width;
        targetSize.height = targetSize.width / aspectRatio;
      }
      projectTo.size = targetSize;
      break;
  }

  projectTo = CGRectIntegral(projectTo);
  // There's no CGSizeIntegral, faking one instead.
  CGRect integralRect = CGRectZero;
  integralRect.size = targetSize;
  targetSize = CGRectIntegral(integralRect).size;
}
