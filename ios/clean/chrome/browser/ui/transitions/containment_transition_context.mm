// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/containment_transition_context.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContainmentTransitionContext ()
@property(nonatomic, strong) UIView* containerView;
@property(nonatomic, strong) UIViewController* parentViewController;
@property(nonatomic, copy) ProceduralBlockWithBool completion;
@property(nonatomic, weak) id<UIViewControllerAnimatedTransitioning> animator;
@property(nonatomic, strong)
    NSDictionary<UITransitionContextViewControllerKey, UIViewController*>*
        viewControllers;
@property(nonatomic, strong)
    NSDictionary<UITransitionContextViewKey, UIView*>* views;
@end

@implementation ContainmentTransitionContext
@synthesize animated = _animated;
@synthesize interactive = _interactive;
@synthesize transitionWasCancelled = _transitionWasCancelled;
@synthesize presentationStyle = _presentationStyle;
@synthesize targetTransform = _targetTransform;
@synthesize containerView = _containerView;
@synthesize parentViewController = _parentViewController;
@synthesize completion = _completion;
@synthesize animator = _animator;
@synthesize viewControllers = _viewControllers;
@synthesize views = _views;

- (instancetype)initWithFromViewController:(UIViewController*)fromViewController
                          toViewController:(UIViewController*)toViewController
                      parentViewController:
                          (UIViewController*)parentViewController
                                    inView:(UIView*)containerView
                                completion:(ProceduralBlockWithBool)completion {
  self = [super init];
  if (self) {
    DCHECK(parentViewController);
    DCHECK(!fromViewController ||
           fromViewController.parentViewController == parentViewController);
    DCHECK(!toViewController || toViewController.parentViewController == nil);
    _animated = YES;
    _interactive = NO;
    _presentationStyle = UIModalPresentationCustom;
    _targetTransform = CGAffineTransformIdentity;
    NSMutableDictionary<UITransitionContextViewControllerKey,
                        UIViewController*>* viewControllers =
        [NSMutableDictionary dictionary];
    NSMutableDictionary<UITransitionContextViewKey, UIView*>* views =
        [NSMutableDictionary dictionary];
    if (fromViewController) {
      viewControllers[UITransitionContextFromViewControllerKey] =
          fromViewController;
      views[UITransitionContextFromViewKey] = fromViewController.view;
    }
    if (toViewController) {
      viewControllers[UITransitionContextToViewControllerKey] =
          toViewController;
      views[UITransitionContextToViewKey] = toViewController.view;
    }
    _viewControllers = [viewControllers copy];
    _views = [views copy];
    _parentViewController = parentViewController;
    _containerView = containerView;
    _completion = [completion copy];
  }
  return self;
}

- (void)prepareTransitionWithAnimator:
    (id<UIViewControllerAnimatedTransitioning>)animator {
  self.animator = animator;
  [self.viewControllers[UITransitionContextFromViewControllerKey]
      willMoveToParentViewController:nil];
  UIViewController* toViewController =
      self.viewControllers[UITransitionContextToViewControllerKey];
  if (toViewController) {
    [self.parentViewController addChildViewController:toViewController];
  }
}

#pragma mark - UIViewControllerContextTransitioning

- (void)updateInteractiveTransition:(CGFloat)percentComplete {
}

- (void)finishInteractiveTransition {
}

- (void)cancelInteractiveTransition {
}

- (void)pauseInteractiveTransition {
}

- (void)completeTransition:(BOOL)didComplete {
  [[self viewForKey:UITransitionContextFromViewKey] removeFromSuperview];
  [[self viewControllerForKey:UITransitionContextFromViewControllerKey]
      removeFromParentViewController];
  [[self viewControllerForKey:UITransitionContextToViewControllerKey]
      didMoveToParentViewController:self.parentViewController];
  if ([self.animator respondsToSelector:@selector(animationEnded:)]) {
    [self.animator animationEnded:didComplete];
  }
  if (self.completion) {
    self.completion(didComplete);
  }
}

- (UIViewController*)viewControllerForKey:
    (UITransitionContextViewControllerKey)key {
  return self.viewControllers[key];
}

- (UIView*)viewForKey:(UITransitionContextViewKey)key {
  return self.views[key];
}

- (CGRect)initialFrameForViewController:(UIViewController*)viewController {
  return CGRectZero;
}

- (CGRect)finalFrameForViewController:(UIViewController*)viewController {
  return CGRectZero;
}

@end
