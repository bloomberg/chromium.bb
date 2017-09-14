// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller+internal.h"

#import "base/logging.h"
#import "ios/clean/chrome/browser/ui/transitions/animators/swap_from_above_animator.h"
#import "ios/clean/chrome/browser/ui/transitions/containment_transition_context.h"
#import "ios/clean/chrome/browser/ui/transitions/containment_transitioning_delegate.h"
#import "ios/clean/chrome/browser/ui/ui_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolbarHeight = 56.0f;
}  // namespace

@interface TabContainerViewController ()<ContainmentTransitioningDelegate>

// Container view enclosing all child view controllers.
@property(nonatomic, strong) UIView* containerView;

// Container views for child view controllers. The child view controller's
// view is added as a subview that fills its container view via autoresizing.
@property(nonatomic, strong) UIView* findBarView;
@property(nonatomic, strong) UIView* toolbarView;
@property(nonatomic, strong) UIView* contentView;

// Status Bar background view. Its size is directly linked to the difference
// between this VC's view topLayoutGuide top anchor and bottom anchor. This
// means that this view will not be displayed on landscape.
@property(nonatomic, strong) UIView* statusBarBackgroundView;

@end

@implementation TabContainerViewController
@synthesize contentViewController = _contentViewController;
@synthesize findBarViewController = _findBarViewController;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize containerView = _containerView;
@synthesize findBarView = _findBarView;
@synthesize toolbarView = _toolbarView;
@synthesize contentView = _contentView;
@synthesize statusBarBackgroundView = _statusBarBackgroundView;
@synthesize containmentTransitioningDelegate =
    _containmentTransitioningDelegate;
@synthesize usesBottomToolbar = _usesBottomToolbar;
@synthesize statusBarBackgroundColor = _statusBarBackgroundColor;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureSubviews];

  NSMutableArray* constraints = [NSMutableArray array];
  [constraints addObjectsFromArray:[self statusBarBackgroundConstraints]];
  [constraints addObjectsFromArray:[self commonToolbarConstraints]];
  [constraints addObjectsFromArray:(self.usesBottomToolbar
                                        ? [self bottomToolbarConstraints]
                                        : [self topToolbarConstraints])];
  [constraints addObjectsFromArray:[self findbarConstraints]];
  [constraints addObjectsFromArray:[self containerConstraints]];
  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - Public properties

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (self.contentViewController == contentViewController)
    return;
  if ([self isViewLoaded]) {
    [self detachChildViewController:self.contentViewController];
    [self attachChildViewController:contentViewController
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
    [self attachChildViewController:toolbarViewController
                          toSubview:self.toolbarView];
  }
  _toolbarViewController = toolbarViewController;
}

- (void)setUsesBottomToolbar:(BOOL)usesBottomToolbar {
  DCHECK(![self isViewLoaded]);
  _usesBottomToolbar = usesBottomToolbar;
}

- (void)setStatusBarBackgroundColor:(UIColor*)statusBarBackgroundColor {
  _statusBarBackgroundColor = statusBarBackgroundColor;
  self.statusBarBackgroundView.backgroundColor = statusBarBackgroundColor;
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

#pragma mark - Methods in Internal category

- (void)attachChildViewController:(UIViewController*)viewController
                        toSubview:(UIView*)subview {
  if (!viewController || !subview)
    return;
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

- (void)configureSubviews {
  self.containmentTransitioningDelegate = self;
  self.containerView = [[UIView alloc] init];
  self.findBarView = [[UIView alloc] init];
  self.toolbarView = [[UIView alloc] init];
  self.contentView = [[UIView alloc] init];
  self.statusBarBackgroundView = [[UIView alloc] init];
  self.containerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.findBarView.translatesAutoresizingMaskIntoConstraints = NO;
  self.toolbarView.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  self.statusBarBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [UIColor blackColor];
  self.findBarView.backgroundColor = [UIColor clearColor];
  self.toolbarView.backgroundColor = [UIColor blackColor];
  self.contentView.backgroundColor = [UIColor blackColor];
  self.statusBarBackgroundView.backgroundColor = self.statusBarBackgroundColor;
  self.findBarView.clipsToBounds = YES;

  [self.view addSubview:self.containerView];
  [self.view addSubview:self.statusBarBackgroundView];
  [self.containerView addSubview:self.toolbarView];
  [self.containerView addSubview:self.contentView];
  // Findbar should have higher z-order than toolbar.
  [self.containerView addSubview:self.findBarView];
  self.findBarView.hidden = YES;

  [self attachChildViewController:self.toolbarViewController
                        toSubview:self.toolbarView];
  [self attachChildViewController:self.contentViewController
                        toSubview:self.contentView];
}

- (Constraints*)containerConstraints {
  return @[
    [self.containerView.topAnchor
        constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor],
    [self.containerView.bottomAnchor
        constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor],
    [self.containerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.containerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

#pragma mark - ContainmentTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForAddingChildController:(UIViewController*)addedChild
                    removingChildController:(UIViewController*)removedChild
                               toController:(UIViewController*)parent {
  return [[SwapFromAboveAnimator alloc] init];
}

#pragma mark - Private methods

// Constraints for the status bar background.
- (Constraints*)statusBarBackgroundConstraints {
  return @[
    [self.statusBarBackgroundView.topAnchor
        constraintEqualToAnchor:self.topLayoutGuide.topAnchor],
    [self.statusBarBackgroundView.bottomAnchor
        constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor],
    [self.statusBarBackgroundView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.statusBarBackgroundView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
}

// Constraints for the findbar.
- (Constraints*)findbarConstraints {
  return @[
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

// Constraints that are shared between topToolbar and bottomToolbar
// configurations.
- (Constraints*)commonToolbarConstraints {
  return @[
    // Toolbar leading, trailing, and height constraints.
    [self.toolbarView.leadingAnchor
        constraintEqualToAnchor:self.containerView.leadingAnchor],
    [self.toolbarView.trailingAnchor
        constraintEqualToAnchor:self.containerView.trailingAnchor],
    [self.toolbarView.heightAnchor constraintEqualToConstant:kToolbarHeight],

    // Content leading and trailing constraints.
    [self.contentView.leadingAnchor
        constraintEqualToAnchor:self.containerView.leadingAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.containerView.trailingAnchor],
  ];
}

// Constraints that configure the toolbar at the top.
- (Constraints*)topToolbarConstraints {
  return @[
    [self.toolbarView.topAnchor
        constraintEqualToAnchor:self.containerView.topAnchor],
    [self.contentView.topAnchor
        constraintEqualToAnchor:self.toolbarView.bottomAnchor],
    [self.contentView.bottomAnchor
        constraintEqualToAnchor:self.containerView.bottomAnchor],
  ];
}

// Constraints that configure the toolbar at the bottom.
- (Constraints*)bottomToolbarConstraints {
  return @[
    [self.contentView.topAnchor
        constraintEqualToAnchor:self.containerView.topAnchor],
    [self.toolbarView.topAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.toolbarView.bottomAnchor
        constraintEqualToAnchor:self.containerView.bottomAnchor],
  ];
}

@end
