// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_presentation_controller.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMaxWidth = 350;
const CGFloat kMaxHeight = 350;
const CGFloat kMinimumMargin = 25;
}  // namespace

@interface IdentityChooserPresentationController ()

@property(nonatomic, strong) UIView* shieldView;

@end

@implementation IdentityChooserPresentationController

@synthesize shieldView = _shieldView;

#pragma mark - UIPresentationController

- (CGRect)frameOfPresentedViewInContainerView {
  CGRect safeAreaFrame = UIEdgeInsetsInsetRect(
      self.containerView.bounds, SafeAreaInsetsForView(self.containerView));

  CGFloat availableWidth = CGRectGetWidth(safeAreaFrame);
  CGFloat availableHeight = CGRectGetHeight(safeAreaFrame);

  CGFloat width = MIN(kMaxWidth, availableWidth - 2 * kMinimumMargin);
  CGFloat height = MIN(kMaxHeight, availableHeight - 2 * kMinimumMargin);

  CGRect presentedViewFrame = safeAreaFrame;
  presentedViewFrame.origin.x += (availableWidth - width) / 2;
  presentedViewFrame.origin.y += (availableHeight - height) / 2;
  presentedViewFrame.size.width = width;
  presentedViewFrame.size.height = height;

  return presentedViewFrame;
}

- (void)presentationTransitionWillBegin {
  self.shieldView = [[UIView alloc] init];
  // TODO(crbug.com/873065): Change this for the real shadow.
  self.shieldView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.2];
  self.shieldView.frame = self.containerView.bounds;
  [self.containerView addSubview:self.shieldView];
  [self.shieldView
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(handleShieldTap)]];

  [self.containerView addSubview:self.presentedViewController.view];
}

- (void)containerViewWillLayoutSubviews {
  self.shieldView.frame = self.containerView.bounds;

  self.presentedViewController.view.frame =
      [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

- (void)handleShieldTap {
  [self.presentedViewController dismissViewControllerAnimated:YES
                                                   completion:nil];
}

@end
