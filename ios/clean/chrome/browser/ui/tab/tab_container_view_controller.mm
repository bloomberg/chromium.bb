// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"

#import "ios/clean/chrome/browser/ui/transitions/animators/swap_from_above_animator.h"
#import "ios/clean/chrome/browser/ui/transitions/containment_transition_context.h"
#import "ios/clean/chrome/browser/ui/transitions/containment_transitioning_delegate.h"
#import "ios/clean/chrome/browser/ui/ui_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kToolbarHeight = 56.0f;
CGFloat kTabStripHeight = 120.0f;
}

@interface TabContainerViewController ()<ContainmentTransitioningDelegate>

// Container views for child view controllers. The child view controller's
// view is added as a subview that fills its container view via autoresizing.
@property(nonatomic, strong) UIView* findBarView;
@property(nonatomic, strong) UIView* tabStripView;
@property(nonatomic, strong) UIView* toolbarView;
@property(nonatomic, strong) UIView* contentView;

// Height constraints for tabStripView and toolbarView.
@property(nonatomic, strong) NSLayoutConstraint* tabStripHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* toolbarHeightConstraint;

// Cache for forwarding methods to child view controllers.
@property(nonatomic, assign) SEL actionToForward;
@property(nonatomic, weak) UIResponder* forwardingTarget;

// Abstract base method for subclasses to implement.
// Returns constraints for tabStrip, toolbar, and content subviews.
- (Constraints*)subviewConstraints;

@end

@implementation TabContainerViewController

@synthesize contentViewController = _contentViewController;
@synthesize findBarView = _findBarView;
@synthesize findBarViewController = _findBarViewController;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize tabStripViewController = _tabStripViewController;
@synthesize tabStripVisible = _tabStripVisible;
@synthesize tabStripView = _tabStripView;
@synthesize toolbarView = _toolbarView;
@synthesize contentView = _contentView;
@synthesize tabStripHeightConstraint = _tabStripHeightConstraint;
@synthesize toolbarHeightConstraint = _toolbarHeightConstraint;
@synthesize actionToForward = _actionToForward;
@synthesize forwardingTarget = _forwardingTarget;
@synthesize containmentTransitioningDelegate =
    _containmentTransitioningDelegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.containmentTransitioningDelegate = self;
  self.findBarView = [[UIView alloc] init];
  self.tabStripView = [[UIView alloc] init];
  self.toolbarView = [[UIView alloc] init];
  self.contentView = [[UIView alloc] init];
  self.findBarView.translatesAutoresizingMaskIntoConstraints = NO;
  self.tabStripView.translatesAutoresizingMaskIntoConstraints = NO;
  self.toolbarView.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [UIColor blackColor];
  self.findBarView.backgroundColor = [UIColor clearColor];
  self.tabStripView.backgroundColor = [UIColor blackColor];
  self.toolbarView.backgroundColor = [UIColor blackColor];
  self.contentView.backgroundColor = [UIColor blackColor];
  self.findBarView.clipsToBounds = YES;

  // Views that are added last have the highest z-order.
  [self.view addSubview:self.tabStripView];
  [self.view addSubview:self.toolbarView];
  [self.view addSubview:self.contentView];
  [self.view addSubview:self.findBarView];
  self.findBarView.hidden = YES;

  [self addChildViewController:self.tabStripViewController
                     toSubview:self.tabStripView];
  [self addChildViewController:self.toolbarViewController
                     toSubview:self.toolbarView];
  [self addChildViewController:self.contentViewController
                     toSubview:self.contentView];

  self.tabStripHeightConstraint =
      [self.tabStripView.heightAnchor constraintEqualToConstant:0.0f];
  self.toolbarHeightConstraint =
      [self.toolbarView.heightAnchor constraintEqualToConstant:0.0f];
  self.toolbarHeightConstraint.priority = UILayoutPriorityDefaultHigh;
  if (self.toolbarViewController) {
    self.toolbarHeightConstraint.constant = kToolbarHeight;
  }

  [NSLayoutConstraint activateConstraints:[self subviewConstraints]];
}

#pragma mark - Public properties

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (self.contentViewController == contentViewController)
    return;
  if ([self isViewLoaded]) {
    [self detachChildViewController:self.contentViewController];
    [self addChildViewController:contentViewController
                       toSubview:self.contentView];
  }
  _contentViewController = contentViewController;
}

- (void)setFindBarViewController:(UIViewController*)findBarViewController {
  if (self.findBarViewController == findBarViewController)
    return;
  if ([self isViewLoaded]) {
    self.findBarView.hidden = NO;
    findBarViewController.view.translatesAutoresizingMaskIntoConstraints = YES;
    findBarViewController.view.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    ContainmentTransitionContext* context =
        [[ContainmentTransitionContext alloc]
            initWithFromViewController:self.findBarViewController
                      toViewController:findBarViewController
                  parentViewController:self
                                inView:self.findBarView
                            completion:^(BOOL finished) {
                              self.findBarView.hidden =
                                  (findBarViewController == nil);
                            }];
    id<UIViewControllerAnimatedTransitioning> animator =
        [self.containmentTransitioningDelegate
            animationControllerForAddingChildController:findBarViewController
                                removingChildController:
                                    self.findBarViewController
                                           toController:self];
    [context prepareTransitionWithAnimator:animator];
    [animator animateTransition:context];
  }
  _findBarViewController = findBarViewController;
}

- (void)setToolbarViewController:(UIViewController*)toolbarViewController {
  if (self.toolbarViewController == toolbarViewController)
    return;
  if ([self isViewLoaded]) {
    [self detachChildViewController:self.toolbarViewController];
    [self addChildViewController:toolbarViewController
                       toSubview:self.toolbarView];
  }
  _toolbarViewController = toolbarViewController;
}

- (void)setTabStripVisible:(BOOL)tabStripVisible {
  if (tabStripVisible) {
    self.tabStripHeightConstraint.constant = kTabStripHeight;
  } else {
    self.tabStripHeightConstraint.constant = 0.0f;
  }
  _tabStripVisible = tabStripVisible;
}

- (void)setTabStripViewController:(UIViewController*)tabStripViewController {
  if (self.tabStripViewController == tabStripViewController)
    return;
  if ([self isViewLoaded]) {
    [self detachChildViewController:self.tabStripViewController];
    [self addChildViewController:tabStripViewController
                       toSubview:self.tabStripView];
  }
  _tabStripViewController = tabStripViewController;
}

#pragma mark - ChildViewController helper methods

- (void)addChildViewController:(UIViewController*)viewController
                     toSubview:(UIView*)subview {
  if (!viewController || !subview) {
    return;
  }
  [self addChildViewController:viewController];
  viewController.view.translatesAutoresizingMaskIntoConstraints = YES;
  viewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  viewController.view.frame = subview.bounds;
  [subview addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
}

- (void)detachChildViewController:(UIViewController*)viewController {
  if (viewController.parentViewController != self)
    return;
  [viewController willMoveToParentViewController:nil];
  [viewController.view removeFromSuperview];
  [viewController removeFromParentViewController];
}

#pragma mark - MenuPresentationDelegate

- (CGRect)boundsForMenuPresentation {
  return self.view.bounds;
}
- (CGRect)originForMenuPresentation {
  return [self rectForZoomWithKey:nil inView:self.view];
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

#pragma mark - Tab Strip actions.

- (void)hideTabStrip:(id)sender {
  self.tabStripVisible = NO;
}

#pragma mark - Abstract methods to be overriden by subclass

- (Constraints*)subviewConstraints {
  [NSException
       raise:NSInternalInconsistencyException
      format:@"You must override %@ in a subclass", NSStringFromSelector(_cmd)];
  return nil;
}

#pragma mark - ContainmentTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForAddingChildController:(UIViewController*)addedChild
                    removingChildController:(UIViewController*)removedChild
                               toController:(UIViewController*)parent {
  return [[SwapFromAboveAnimator alloc] init];
}

@end

@implementation TopToolbarTabViewController

// Override with constraints that place the toolbar on top.
- (Constraints*)subviewConstraints {
  return @[
    [self.tabStripView.topAnchor
        constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor],
    [self.tabStripView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.tabStripView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    self.tabStripHeightConstraint,
    [self.toolbarView.topAnchor
        constraintEqualToAnchor:self.tabStripView.bottomAnchor],
    [self.toolbarView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.toolbarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    self.toolbarHeightConstraint,
    [self.contentView.topAnchor
        constraintEqualToAnchor:self.toolbarView.bottomAnchor],
    [self.contentView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.contentView.bottomAnchor
        constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor],

    [self.findBarView.topAnchor
        constraintEqualToAnchor:self.toolbarView.topAnchor],
    [self.findBarView.bottomAnchor
        constraintEqualToAnchor:self.toolbarView.bottomAnchor],
    [self.findBarView.leadingAnchor
        constraintEqualToAnchor:self.toolbarView.leadingAnchor],
    [self.findBarView.trailingAnchor
        constraintEqualToAnchor:self.toolbarView.trailingAnchor],
  ];
}

@end

@implementation BottomToolbarTabViewController

// Override with constraints that place the toolbar on bottom.
- (Constraints*)subviewConstraints {
  return @[
    [self.tabStripView.topAnchor
        constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor],
    [self.tabStripView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.tabStripView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    self.tabStripHeightConstraint,
    [self.contentView.topAnchor
        constraintEqualToAnchor:self.tabStripView.bottomAnchor],
    [self.contentView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.toolbarView.topAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.toolbarView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.toolbarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    self.toolbarHeightConstraint,
    [self.toolbarView.bottomAnchor
        constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor],
  ];
}

@end
