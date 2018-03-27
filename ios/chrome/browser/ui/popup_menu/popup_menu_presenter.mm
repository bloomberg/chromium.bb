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
const CGFloat kMinWidth = 200;
const CGFloat kMaxWidth = 300;
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

  // Set the frame of the table view to the maximum width to have the label
  // resizing correctly.
  CGRect frame = self.presentedViewController.view.frame;
  frame.size.width = kMaxWidth - 2 * kMinMargin;
  self.presentedViewController.view.frame = frame;
  // It is necessary to do a first layout pass so the table view can size
  // itself.
  [self.presentedViewController.view setNeedsLayout];
  [self.presentedViewController.view layoutIfNeeded];
  CGSize fittingSize = [self.presentedViewController.view
      sizeThatFits:CGSizeMake(kMaxWidth, kMaxHeight)];
  // Use preferredSize if it is set.
  CGSize preferredSize = self.presentedViewController.preferredContentSize;
  CGFloat width = fittingSize.width;
  if (!CGSizeEqualToSize(preferredSize, CGSizeZero)) {
    width = preferredSize.width;
  }

  // Set the sizing constraints, in case the UIViewController is using a
  // UIScrollView. The priority needs to be non-required to allow downsizing if
  // needed, and more than UILayoutPriorityDefaultHigh to take precedence on
  // compression resistance.
  NSLayoutConstraint* widthConstraint =
      [self.presentedViewController.view.widthAnchor
          constraintEqualToConstant:width];
  widthConstraint.priority = UILayoutPriorityDefaultHigh + 1;
  widthConstraint.active = YES;

  NSLayoutConstraint* heightConstraint =
      [self.presentedViewController.view.heightAnchor
          constraintEqualToConstant:fittingSize.height];
  heightConstraint.priority = UILayoutPriorityDefaultHigh + 1;
  heightConstraint.active = YES;

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
    [container.widthAnchor constraintGreaterThanOrEqualToConstant:kMinWidth],
    [container.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeArea.bottomAnchor
                                 constant:-kMinMargin],
    [container.topAnchor constraintGreaterThanOrEqualToAnchor:safeArea.topAnchor
                                                     constant:kMinMargin],
  ]];
  NSLayoutConstraint* leading = [container.leadingAnchor
      constraintEqualToAnchor:namedGuide.leadingAnchor];
  leading.priority = UILayoutPriorityDefaultHigh;
  leading.active = YES;
}

@end
