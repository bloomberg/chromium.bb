// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_controller.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The presented view Height
const CGFloat kPresentedViewHeight = 267.0;
// The presented view outer horizontal margins.
const CGFloat kPresentedViewHorizontalMargin = 10.0;
// The presented view maximum width.
const CGFloat kPresentedViewMaxWidth = 394.0;
// The rounded corner radius for the container view.
const CGFloat kContainerCornerRadius = 13.0;
// The background colot for the container view.
const int kContainerBackgroundColor = 0x2F2F2F;
// The alpha component for the container view background color.
const CGFloat kContainerBackgroundColorAlpha = 0.5;
}  // namespace

@implementation InfobarModalPresentationController

- (void)containerViewWillLayoutSubviews {
  self.presentedView.frame = [self frameForPresentedView];

  // Style the presented and container views.
  self.presentedView.layer.cornerRadius = kContainerCornerRadius;
  self.presentedView.layer.masksToBounds = YES;
  self.presentedView.clipsToBounds = YES;
  self.containerView.backgroundColor =
      [UIColorFromRGB(kContainerBackgroundColor)
          colorWithAlphaComponent:kContainerBackgroundColorAlpha];
}

- (CGRect)frameForPresentedView {
  CGFloat containerWidth = CGRectGetWidth(self.containerView.bounds);
  CGFloat containerHeight = CGRectGetHeight(self.containerView.bounds);

  // Calculate the frame width.
  CGFloat maxAvailableWidth =
      containerWidth - 2 * kPresentedViewHorizontalMargin;
  CGFloat frameWidth = fmin(maxAvailableWidth, kPresentedViewMaxWidth);

  // Based on the container width calculate the values in order to center the
  // frame in the X and Y axis.
  CGFloat modalXPosition = (containerWidth / 2) - (frameWidth / 2);
  CGFloat modalYPosition = (containerHeight / 2) - (kPresentedViewHeight / 2);

  return CGRectMake(modalXPosition, modalYPosition, frameWidth,
                    kPresentedViewHeight);
}

@end
