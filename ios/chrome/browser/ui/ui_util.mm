// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_util.h"

#import <UIKit/UIKit.h>
#include <limits>

#include "base/feature_list.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_private_base_feature.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/device_form_factor.h"
#include "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsIPadIdiom() {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}

const CGFloat kPortraitWidth[INTERFACE_IDIOM_COUNT] = {
    320,  // IPHONE_IDIOM
    768   // IPAD_IDIOM
};

bool IsHighResScreen() {
  return [[UIScreen mainScreen] scale] > 1.0;
}

bool IsPortrait() {
  UIInterfaceOrientation orient = GetInterfaceOrientation();
  return UIInterfaceOrientationIsPortrait(orient) ||
         orient == UIInterfaceOrientationUnknown;
}

bool IsLandscape() {
  return UIInterfaceOrientationIsLandscape(GetInterfaceOrientation());
}

CGFloat CurrentScreenHeight() {
  return [UIScreen mainScreen].bounds.size.height;
}

CGFloat CurrentScreenWidth() {
  return [UIScreen mainScreen].bounds.size.width;
}

bool IsIPhoneX() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return (idiom == UIUserInterfaceIdiomPhone &&
          CGRectGetHeight([[UIScreen mainScreen] nativeBounds]) == 2436);
}

bool IsSafeAreaCompatibleToolbarEnabled() {
  return (IsIPhoneX() &&
          base::FeatureList::IsEnabled(kSafeAreaCompatibleToolbar)) ||
         base::FeatureList::IsEnabled(kCleanToolbar);
}

CGFloat StatusBarHeight() {
  // This is a temporary solution until usage of StatusBarHeight has been
  // replaced with topLayoutGuide.

  if (IsIPhoneX()) {
    if (IsSafeAreaCompatibleToolbarEnabled()) {
      return IsPortrait() ? 44 : 0;
    } else {
      // Return the height of the portrait status bar even in landscape because
      // the Toolbar does not properly layout itself if the status bar height
      // changes.
      return 44;
    }
  }

  // Checking [UIApplication sharedApplication].statusBarFrame will return the
  // wrong offset when the application is started while in a phone call, so
  // simply return 20 here.
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

bool AreCGFloatsEqual(CGFloat a, CGFloat b) {
  return std::fabs(a - b) <= std::numeric_limits<CGFloat>::epsilon();
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
    case ProjectionMode::kAspectFillAlignTop:
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
      if (projectionMode == ProjectionMode::kAspectFillAlignTop) {
        projectTo.origin.y = 0;
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
