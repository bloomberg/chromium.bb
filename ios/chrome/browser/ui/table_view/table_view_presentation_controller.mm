// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The rounded corner radius for the bubble container, in Regular widths.
const CGFloat kContainerCornerRadius = 13.0;
}

@interface TableViewPresentationController ()

@property(nonatomic, readwrite, strong) UIVisualEffectView* tableViewContainer;

@end

@implementation TableViewPresentationController
@synthesize tableViewContainer = _tableViewContainer;

- (CGRect)frameOfPresentedViewInContainerView {
  // TODO(crbug.com/820154): The constants below are arbitrary and need to be
  // replaced with final values.
  return UIEdgeInsetsInsetRect(self.containerView.frame,
                               UIEdgeInsetsMake(90, 200, 60, 10));
}

- (UIView*)presentedView {
  return self.tableViewContainer;
}

- (void)presentationTransitionWillBegin {
  self.tableViewContainer = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect
                         effectWithStyle:UIBlurEffectStyleExtraLight]];
  self.tableViewContainer.contentView.backgroundColor =
      [UIColor colorWithHue:0 saturation:0 brightness:0.98 alpha:0.95];
  [self.tableViewContainer.contentView
      addSubview:self.presentedViewController.view];

  self.tableViewContainer.layer.cornerRadius = kContainerCornerRadius;
  self.tableViewContainer.layer.masksToBounds = YES;
  self.tableViewContainer.clipsToBounds = YES;
  [self.containerView addSubview:self.tableViewContainer];

  self.tableViewContainer.frame = [self frameOfPresentedViewInContainerView];
  self.presentedViewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.presentedViewController.view.frame =
      self.tableViewContainer.contentView.bounds;
}

- (void)containerViewWillLayoutSubviews {
  self.tableViewContainer.frame = [self frameOfPresentedViewInContainerView];
}

- (void)dismissalTransitionDidEnd:(BOOL)completed {
  if (completed) {
    [self.tableViewContainer removeFromSuperview];
    self.tableViewContainer = nil;
  }
}

#pragma mark - Adaptivity

- (UIModalPresentationStyle)adaptivePresentationStyleForTraitCollection:
    (UITraitCollection*)traitCollection {
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    return UIModalPresentationFullScreen;
  }

  return UIModalPresentationNone;
}

@end
