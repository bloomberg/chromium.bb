// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The presented view height.
const CGFloat kDefaultContainerHeight = 70;
// The presented view outer horizontal margins.
const CGFloat kContainerHorizontalPadding = 8;
// The presented view maximum width.
const CGFloat kContainerWidthMaxSize = 398;
// The presented view default Y axis value.
const CGFloat kDefaultBannerYPosition = 85;
}

@implementation InfobarBannerPresentationController

- (void)presentationTransitionWillBegin {
  self.containerView.frame = [self viewForPresentedView].frame;
}

- (void)containerViewWillLayoutSubviews {
  self.containerView.frame = [self viewForPresentedView].frame;
  self.presentedView.frame = [self viewForPresentedView].bounds;
}

- (UIView*)viewForPresentedView {
  UIWindow* window = UIApplication.sharedApplication.keyWindow;

  // Calculate the Banner container width.
  CGFloat safeAreaWidth = CGRectGetWidth(window.bounds);
  CGFloat maxAvailableWidth = safeAreaWidth - 2 * kContainerHorizontalPadding;
  CGFloat frameWidth = fmin(maxAvailableWidth, kContainerWidthMaxSize);

  // Based on the container width, calculate the value in order to center the
  // Banner in the X axis.
  CGFloat bannerXPosition = (safeAreaWidth / 2) - (frameWidth / 2);

  CGFloat bannerYPosition = [self.bannerPositioner bannerYPosition];
  if (!bannerYPosition) {
    bannerYPosition = kDefaultBannerYPosition;
  }

  return [[UIView alloc]
      initWithFrame:CGRectMake(bannerXPosition, bannerYPosition, frameWidth,
                               kDefaultContainerHeight)];
}

@end
