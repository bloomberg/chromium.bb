// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Presented frame size and position.
const CGFloat kContainerHeight = 75;
const CGFloat kContainerHorizontalPadding = 20;
const CGFloat kContainerTopPadding = 80;

}

@interface InfobarBannerPresentationController ()

// UIView that contains information about the position and size of the container
// and presented views.
@property(nonatomic, strong) UIView* viewForPresentedView;

@end

@implementation InfobarBannerPresentationController

- (CGRect)frameOfPresentedViewInContainerView {
  return self.viewForPresentedView.bounds;
}

- (void)presentationTransitionWillBegin {
  self.containerView.frame = self.viewForPresentedView.frame;
}

- (void)containerViewWillLayoutSubviews {
  self.presentedView.frame = [self frameOfPresentedViewInContainerView];
}

- (UIView*)viewForPresentedView {
  if (!_viewForPresentedView) {
    CGFloat safeAreaWidth =
        CGRectGetWidth(self.containerView.safeAreaLayoutGuide.layoutFrame);
    CGFloat maxAvailableWidth = safeAreaWidth - 2 * kContainerHorizontalPadding;
    _viewForPresentedView = [[UIView alloc]
        initWithFrame:CGRectMake(kContainerHorizontalPadding,
                                 kContainerTopPadding, maxAvailableWidth,
                                 kContainerHeight)];
  }
  return _viewForPresentedView;
}

@end
