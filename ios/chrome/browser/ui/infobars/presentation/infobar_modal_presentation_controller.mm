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
// The presented view origin on the X coordinate system of the parent view.
const CGFloat kPresentedViewOriginX = 10.0;
// The presented view origin on the Y coordinate system of the parent view.
const CGFloat kPresentedViewOriginY = 277.0;
// The rounded corner radius for the container view.
const CGFloat kContainerCornerRadius = 13.0;
// The background colot for the container view.
const int kContainerBackgroundColor = 0x2F2F2F;
// The alpha component for the container view background color.
const CGFloat kContainerBackgroundColorAlpha = 0.5;
}  // namespace

@implementation InfobarModalPresentationController

// TODO(crbug.com/1372916): Placeholder size and position for the presented
// view.
- (void)containerViewWillLayoutSubviews {
  CGRect safeAreaBounds = self.containerView.safeAreaLayoutGuide.layoutFrame;
  CGFloat safeAreaWidth = CGRectGetWidth(safeAreaBounds);
  CGFloat maxAvailableWidth =
      safeAreaWidth - 2 * kPresentedViewHorizontalMargin;
  self.presentedView.frame =
      CGRectMake(kPresentedViewOriginX, kPresentedViewOriginY,
                 maxAvailableWidth, kPresentedViewHeight);

  self.presentedView.layer.cornerRadius = kContainerCornerRadius;
  self.presentedView.layer.masksToBounds = YES;
  self.presentedView.clipsToBounds = YES;

  UIColor* backgroundColor = UIColorFromRGB(kContainerBackgroundColor);
  self.containerView.backgroundColor =
      [backgroundColor colorWithAlphaComponent:kContainerBackgroundColorAlpha];
}

@end
