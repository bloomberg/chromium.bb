// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_container_view_controller.h"

#import "ios/clean/chrome/browser/ui/ui_types.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kStripHeight = 200.0;
}

@interface TabStripContainerViewController ()<TabStripActions>

// Whichever view controller is at the top of the screen. This view controller
// controls the status bar.
@property(nonatomic, weak) UIViewController* topmostViewController;

@property(nonatomic, strong) Constraints* contentConstraintsWithStrip;
@property(nonatomic, strong) Constraints* contentConstraintsWithoutStrip;
@property(nonatomic, strong) Constraints* stripConstraints;

// Cache for forwarding methods to child view controllers.
@property(nonatomic, assign) SEL actionToForward;
@property(nonatomic, weak) UIResponder* forwardingTarget;

@property(nonatomic, strong) NSLayoutConstraint* stripHeightConstraint;

// Contained view controller utility methods.
- (void)removeChildViewController:(UIViewController*)viewController;

// Called after a new content view controller is set, but before
// |-didMoveToParentViewController:| is called on that view controller.
- (void)didAddContentViewController;

// Called after a new strip view controller is set, but before
// |-didMoveToParentViewController:| is called on that view controller.
- (void)didAddStripViewController;

// Methods to populate the constraint properties.
- (void)updateContentConstraintsWithStrip;
- (void)updateContentConstraintsWithoutStrip;
- (void)updateStripConstraints;

@end

@implementation TabStripContainerViewController

@synthesize contentViewController = _contentViewController;
@synthesize tabStripViewController = _tabStripViewController;
@synthesize topmostViewController = _topmostViewController;
@synthesize contentConstraintsWithStrip = _contentConstraintsWithStrip;
@synthesize contentConstraintsWithoutStrip = _contentConstraintsWithoutStrip;
@synthesize stripConstraints = _stripConstraints;
@synthesize actionToForward = _actionToForward;
@synthesize forwardingTarget = _forwardingTarget;
@synthesize stripHeightConstraint = _stripHeightConstraint;

#pragma mark - Public properties

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (self.contentViewController == contentViewController)
    return;

  // Remove the current content view controller, if any.
  [NSLayoutConstraint
      deactivateConstraints:self.contentConstraintsWithoutStrip];
  [NSLayoutConstraint deactivateConstraints:self.contentConstraintsWithStrip];
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

- (void)setStripViewController:(UIViewController*)tabStripViewController {
  if (self.tabStripViewController == tabStripViewController)
    return;

  // Remove the current strip view controller, if any.
  [NSLayoutConstraint deactivateConstraints:self.stripConstraints];
  [NSLayoutConstraint deactivateConstraints:self.contentConstraintsWithStrip];
  [self removeChildViewController:self.tabStripViewController];

  // Add the new strip view controller.
  [self addChildViewController:tabStripViewController];
  tabStripViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:tabStripViewController.view];
  _tabStripViewController = tabStripViewController;
  [self didAddStripViewController];
  [self.view setNeedsUpdateConstraints];
  [self.tabStripViewController didMoveToParentViewController:self];
}

#pragma mark - UIViewController

- (void)updateViewConstraints {
  if (self.tabStripViewController) {
    [NSLayoutConstraint activateConstraints:self.stripConstraints];
    [NSLayoutConstraint activateConstraints:self.contentConstraintsWithStrip];
  } else {
    [NSLayoutConstraint
        activateConstraints:self.contentConstraintsWithoutStrip];
  }
  [super updateViewConstraints];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.topmostViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.topmostViewController;
}

#pragma mark - UIResponder

// Before forwarding actions up the responder chain, give both contained
// view controllers a chance to handle them.
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  self.actionToForward = nullptr;
  self.forwardingTarget = nil;
  for (UIResponder* responder in
       @[ self.contentViewController, self.tabStripViewController ]) {
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

#pragma mark - TabStripActions

// Action to toggle visibility of tab strip.
- (void)toggleTabStrip:(id)sender {
  self.stripHeightConstraint.constant =
      self.stripHeightConstraint.constant > 0 ? 0.0 : kStripHeight;
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
  if (self.tabStripViewController) {
    [self updateContentConstraintsWithStrip];
  } else {
    self.topmostViewController = self.contentViewController;
    [self updateContentConstraintsWithoutStrip];
  }
}

- (void)didAddStripViewController {
  [self updateStripConstraints];
  // If there's already a content view controller, update the constraints for
  // that, too.
  if (self.contentViewController) {
    [self updateContentConstraintsWithStrip];
  }
  self.topmostViewController = self.tabStripViewController;
}

- (void)updateContentConstraintsWithoutStrip {
  UIView* contentView = self.contentViewController.view;
  self.contentConstraintsWithoutStrip = @[
    [contentView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [contentView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

- (void)updateContentConstraintsWithStrip {
  UIView* contentView = self.contentViewController.view;
  self.contentConstraintsWithStrip = @[
    [contentView.topAnchor
        constraintEqualToAnchor:self.tabStripViewController.view.bottomAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [contentView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

- (void)updateStripConstraints {
  UIView* stripView = self.tabStripViewController.view;
  self.stripHeightConstraint =
      [stripView.heightAnchor constraintEqualToConstant:0.0];
  self.stripConstraints = @[
    [stripView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [stripView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [stripView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    self.stripHeightConstraint,
  ];
}

@end
