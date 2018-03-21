// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_presenter.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMinHeight = 200;
const CGFloat kMaxWidth = 250;
const CGFloat kMaxHeight = 400;
const CGFloat kMinMargin = 16;
}  // namespace

@interface PopupMenuPresenter ()
@property(nonatomic, strong) PopupMenuViewController* popupViewController;
@end

@implementation PopupMenuPresenter

@synthesize baseViewController = _baseViewController;
@synthesize commandHandler = _commandHandler;
@synthesize delegate = _delegate;
@synthesize guideName = _guideName;
@synthesize popupViewController = _popupViewController;
@synthesize presentedViewController = _presentedViewController;

#pragma mark - Public

- (void)prepareForPresentation {
  DCHECK(self.baseViewController);
  if (self.popupViewController)
    return;

  self.popupViewController = [[PopupMenuViewController alloc] init];
  self.popupViewController.commandHandler = self.commandHandler;
  [self.presentedViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self.popupViewController addContent:self.presentedViewController];

  [self.baseViewController addChildViewController:self.popupViewController];
  [self.baseViewController.view addSubview:self.popupViewController.view];
  self.popupViewController.view.frame = self.baseViewController.view.bounds;

  // TODO(crbug.com/804774): Prepare for animation.
  self.popupViewController.contentContainer.alpha = 0;
  [self positionPopupOnNamedGuide];

  [self.popupViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  // TODO(crbug.com/804774): Add animation based on |guideName|.
  self.popupViewController.contentContainer.alpha = 1;
  self.popupViewController.contentContainer.frame =
      CGRectMake(170, 300, 230, 400);
}

- (void)dismissAnimated:(BOOL)animated {
  [self.popupViewController willMoveToParentViewController:nil];
  // TODO(crbug.com/804771): Add animation.
  [self.popupViewController.view removeFromSuperview];
  [self.popupViewController removeFromParentViewController];
  self.popupViewController = nil;
  [self.delegate containedPresenterDidDismiss:self];
}

#pragma mark - Private

// Positions the popup relatively to the |guideName| layout guide. The popup is
// positioned closest to the layout guide, by default it is presented below the
// layout guide, aligned on its leading edge. However, it is respecting the safe
// area bounds.
- (void)positionPopupOnNamedGuide {
  UIView* parentView = self.baseViewController.view;
  UIView* container = self.popupViewController.contentContainer;

  UILayoutGuide* namedGuide =
      [NamedGuide guideWithName:self.guideName view:parentView];
  CGRect guideFrame =
      [self.popupViewController.view convertRect:namedGuide.layoutFrame
                                        fromView:namedGuide.owningView];

  if (CGRectGetMaxY(guideFrame) + kMinHeight >
      CGRectGetHeight(parentView.frame)) {
    // Display above.
    [container.bottomAnchor constraintEqualToAnchor:namedGuide.topAnchor]
        .active = YES;
  } else {
    // Display below.
    [container.topAnchor constraintEqualToAnchor:namedGuide.bottomAnchor]
        .active = YES;
  }

  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(parentView);
  [NSLayoutConstraint activateConstraints:@[
    [container.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.leadingAnchor
                                    constant:kMinMargin],
    [container.trailingAnchor
        constraintLessThanOrEqualToAnchor:safeArea.trailingAnchor
                                 constant:-kMinMargin],
    [container.heightAnchor constraintLessThanOrEqualToConstant:kMaxHeight],
    [container.widthAnchor constraintLessThanOrEqualToConstant:kMaxWidth],
    [container.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeArea.bottomAnchor],
    [container.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.topAnchor],
  ]];
  NSLayoutConstraint* leading = [container.leadingAnchor
      constraintEqualToAnchor:namedGuide.leadingAnchor];
  leading.priority = UILayoutPriorityDefaultHigh;
  leading.active = YES;
}

@end
