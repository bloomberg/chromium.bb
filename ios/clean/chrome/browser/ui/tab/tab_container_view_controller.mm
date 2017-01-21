// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/clean/chrome/browser/ui/ui_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kToolbarHeight = 56.0;
}

@interface TabContainerViewController ()

// Whichever view controller is at the top of the screen. This view controller
// controls the status bar.
@property(nonatomic, weak) UIViewController* topmostViewController;

@property(nonatomic, strong) Constraints* contentConstraintsWithToolbar;
@property(nonatomic, strong) Constraints* contentConstraintsWithoutToolbar;
@property(nonatomic, strong) Constraints* toolbarConstraints;

// Cache for forwarding methods to child view controllers.
@property(nonatomic, assign) SEL actionToForward;
@property(nonatomic, weak) UIResponder* forwardingTarget;

// Contained view controller utility methods.
- (void)removeChildViewController:(UIViewController*)viewController;

// Called after a new content view controller is set, but before
// |-didMoveToParentViewController:| is called on that view controller.
- (void)didAddContentViewController;

// Called after a new toolbar view controller is set, but before
// |-didMoveToParentViewController:| is called on that view controller.
- (void)didAddToolbarViewController;

// Methods to populate the constraint properties.
- (void)updateContentConstraintsWithToolbar;
- (void)updateContentConstraintsWithoutToolbar;
- (void)updateToolbarConstraints;

@end

@implementation TabContainerViewController

@synthesize contentViewController = _contentViewController;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize topmostViewController = _topmostViewController;
@synthesize contentConstraintsWithToolbar = _contentConstraintsWithToolbar;
@synthesize contentConstraintsWithoutToolbar =
    _contentConstraintsWithoutToolbar;
@synthesize toolbarConstraints = _toolbarConstraints;
@synthesize actionToForward = _actionToForward;
@synthesize forwardingTarget = _forwardingTarget;

#pragma mark - Public properties

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (self.contentViewController == contentViewController)
    return;

  // Remove the current content view controller, if any.
  [NSLayoutConstraint
      deactivateConstraints:self.contentConstraintsWithoutToolbar];
  [NSLayoutConstraint deactivateConstraints:self.contentConstraintsWithToolbar];
  [self removeChildViewController:self.contentViewController];

  // Add the new content view controller.
  [self addChildViewController:contentViewController];
  contentViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:contentViewController.view];
  _contentViewController = contentViewController;
  [self didAddContentViewController];
  [self.view setNeedsUpdateConstraints];
  [self.contentViewController didMoveToParentViewController:self];
}

- (void)setToolbarViewController:(UIViewController*)toolbarViewController {
  if (self.toolbarViewController == toolbarViewController)
    return;

  // Remove the current toolbar view controller, if any.
  [NSLayoutConstraint deactivateConstraints:self.toolbarConstraints];
  [NSLayoutConstraint deactivateConstraints:self.contentConstraintsWithToolbar];
  [self removeChildViewController:self.toolbarViewController];

  // Add the new toolbar view controller.
  [self addChildViewController:toolbarViewController];
  toolbarViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:toolbarViewController.view];
  _toolbarViewController = toolbarViewController;
  [self didAddToolbarViewController];
  [self.view setNeedsUpdateConstraints];
  [self.toolbarViewController didMoveToParentViewController:self];
}

#pragma mark - UIViewController

- (void)updateViewConstraints {
  if (self.toolbarViewController) {
    [NSLayoutConstraint activateConstraints:self.toolbarConstraints];
    [NSLayoutConstraint activateConstraints:self.contentConstraintsWithToolbar];
  } else {
    [NSLayoutConstraint
        activateConstraints:self.contentConstraintsWithoutToolbar];
  }
  [super updateViewConstraints];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.topmostViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.topmostViewController;
}

#pragma mark - MenuPresentationDelegate

- (CGRect)frameForMenuPresentation:(UIPresentationController*)presentation {
  CGSize menuSize = presentation.presentedView.frame.size;
  CGRect menuRect;
  menuRect.size = menuSize;

  CGRect menuOriginRect = [self rectForZoomWithKey:@"" inView:self.view];
  if (CGRectIsNull(menuOriginRect)) {
    menuRect.origin = CGPointMake(50, 50);
    return menuRect;
  }
  // Calculate which corner of the menu the origin rect is in. This is
  // determined by comparing frames, and thus is RTL-independent.
  if (CGRectGetMinX(menuOriginRect) - CGRectGetMinX(self.view.bounds) <
      CGRectGetMaxX(self.view.bounds) - CGRectGetMaxX(menuOriginRect)) {
    // Origin rect is closer to the left edge of |self.view| than to the right.
    menuRect.origin.x = CGRectGetMinX(menuOriginRect);
  } else {
    // Origin rect is closer to the right edge of |self.view| than to the left.
    menuRect.origin.x = CGRectGetMaxX(menuOriginRect) - menuSize.width;
  }

  if (CGRectGetMinY(menuOriginRect) - CGRectGetMinY(self.view.bounds) <
      CGRectGetMaxY(self.view.bounds) - CGRectGetMaxY(menuOriginRect)) {
    // Origin rect is closer to the top edge of |self.view| than to the bottom.
    menuRect.origin.y = CGRectGetMinY(menuOriginRect);
  } else {
    // Origin rect is closer to the bottom edge of |self.view| than to the top.
    menuRect.origin.y = CGRectGetMaxY(menuOriginRect) - menuSize.height;
  }

  return menuRect;
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  if ([self.toolbarViewController
          conformsToProtocol:@protocol(ZoomTransitionDelegate)]) {
    return [reinterpret_cast<id<ZoomTransitionDelegate>>(
        self.toolbarViewController) rectForZoomWithKey:key
                                                inView:view];
  }
  return CGRectNull;
}

#pragma mark - UIResponder

// Before forwarding actions up the responder chain, give both contained
// view controllers a chance to handle them.
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  self.actionToForward = nullptr;
  self.forwardingTarget = nil;
  for (UIResponder* responder in
       @[ self.contentViewController, self.toolbarViewController ]) {
    if ([responder canPerformAction:action withSender:sender]) {
      self.actionToForward = action;
      self.forwardingTarget = responder;
      return YES;
    }
  }
  return [super canPerformAction:action withSender:sender];
}

#pragma mark - NSObject method forwarding

- (id)forwardingTargetForSelector:(SEL)aSelector {
  if (aSelector == self.actionToForward) {
    return self.forwardingTarget;
  }
  return nil;
}

#pragma mark - Private methods

- (void)removeChildViewController:(UIViewController*)viewController {
  if (viewController.parentViewController != self)
    return;
  [viewController willMoveToParentViewController:nil];
  [viewController.view removeFromSuperview];
  [viewController removeFromParentViewController];
}

- (void)didAddContentViewController {
  if (self.toolbarViewController) {
    [self updateContentConstraintsWithToolbar];
  } else {
    self.topmostViewController = self.contentViewController;
    [self updateContentConstraintsWithoutToolbar];
  }
}

- (void)didAddToolbarViewController {
  [self updateToolbarConstraints];
  // If there's already a content view controller, update the constraints for
  // that, too.
  if (self.contentViewController) {
    [self updateContentConstraintsWithToolbar];
  }
}

- (void)updateContentConstraintsWithToolbar {
  // Template method for subclasses to implement;
}

- (void)updateToolbarConstraints {
  // Template method for subclasses to implement;
}

- (void)updateContentConstraintsWithoutToolbar {
  UIView* contentView = self.contentViewController.view;
  self.contentConstraintsWithoutToolbar = @[
    [contentView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [contentView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

@end

@implementation TopToolbarTabViewController

- (void)didAddContentViewController {
  [super didAddContentViewController];
  if (!self.toolbarViewController) {
    self.topmostViewController = self.contentViewController;
  }
}

- (void)didAddToolbarViewController {
  [super didAddToolbarViewController];
  self.topmostViewController = self.toolbarViewController;
}

- (void)updateContentConstraintsWithToolbar {
  UIView* contentView = self.contentViewController.view;
  self.contentConstraintsWithToolbar = @[
    [contentView.topAnchor
        constraintEqualToAnchor:self.toolbarViewController.view.bottomAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [contentView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

- (void)updateToolbarConstraints {
  UIView* toolbarView = self.toolbarViewController.view;
  self.toolbarConstraints = @[
    [toolbarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [toolbarView.heightAnchor constraintEqualToConstant:kToolbarHeight],
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [toolbarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

@end

@implementation BottomToolbarTabViewController

- (void)didAddContentViewController {
  [super didAddContentViewController];
  self.topmostViewController = self.contentViewController;
}

// Note that this class doesn't override -didAddToolbarViewController; in the
// case where there is a toolbar view controller set but not a content view
// controller, functionally there is no topmost view controller, so no
// additional action needs to be taken.

- (void)updateContentConstraintsWithToolbar {
  UIView* contentView = self.contentViewController.view;
  self.contentConstraintsWithToolbar = @[
    [contentView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [contentView.bottomAnchor
        constraintEqualToAnchor:self.toolbarViewController.view.topAnchor],
    [contentView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

- (void)updateToolbarConstraints {
  UIView* toolbarView = self.toolbarViewController.view;
  self.toolbarConstraints = @[
    [toolbarView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [toolbarView.heightAnchor constraintEqualToConstant:kToolbarHeight],
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [toolbarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

@end
