// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/containment_transition_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeAnimator : NSObject<UIViewControllerAnimatedTransitioning>

@property(nonatomic) BOOL transitionDurationCalled;
@property(nonatomic) BOOL animateTransitionCalled;
@property(nonatomic) NSUInteger animationEndedCount;
@property(nonatomic) BOOL animationEndedArgument;

@end

@implementation FakeAnimator
@synthesize transitionDurationCalled = _transitionDurationCalled;
@synthesize animateTransitionCalled = _animateTransitionCalled;
@synthesize animationEndedCount = _animationEndedCount;
@synthesize animationEndedArgument = _animationEndedArgument;

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  self.transitionDurationCalled = YES;
  return 0;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  self.animateTransitionCalled = YES;
}

- (void)animationEnded:(BOOL)transitionCompleted {
  self.animationEndedArgument = transitionCompleted;
  self.animationEndedCount++;
}

@end

@interface TestViewController : UIViewController
// Defaults to NO. Set to start recording calls to containment calls.
@property(nonatomic, getter=isRecordingCalls) BOOL recordingCalls;
@property(nonatomic) NSUInteger willMoveToParentCount;
@property(nonatomic) UIViewController* willMoveToParentArgument;
@property(nonatomic) NSUInteger didMoveToParentCount;
@property(nonatomic) UIViewController* didMoveToParentArgument;
@end

@implementation TestViewController
@synthesize recordingCalls = _recordingCalls;
@synthesize willMoveToParentCount = _willMoveToParentCount;
@synthesize willMoveToParentArgument = _willMoveToParentArgument;
@synthesize didMoveToParentCount = _didMoveToParentCount;
@synthesize didMoveToParentArgument = _didMoveToParentArgument;

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  if (self.recordingCalls) {
    self.willMoveToParentCount++;
    self.willMoveToParentArgument = parent;
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (self.recordingCalls) {
    self.didMoveToParentCount++;
    self.didMoveToParentArgument = parent;
  }
}

@end

using ContainmentTransitionContextTest = PlatformTest;

// Tests that the view controllers are properly returned by the context after
// initialization.
TEST_F(ContainmentTransitionContextTest, InitializationChildViewControllers) {
  UIViewController* fromViewController = [[UIViewController alloc] init];
  UIViewController* toViewController = [[UIViewController alloc] init];
  UIViewController* parentViewController = [[UIViewController alloc] init];
  [parentViewController addChildViewController:fromViewController];
  [parentViewController.view addSubview:fromViewController.view];
  [fromViewController didMoveToParentViewController:parentViewController];

  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:fromViewController
                toViewController:toViewController
            parentViewController:parentViewController
                          inView:nil
                      completion:nil];

  EXPECT_EQ(
      fromViewController,
      [context viewControllerForKey:UITransitionContextFromViewControllerKey]);
  EXPECT_EQ(
      toViewController,
      [context viewControllerForKey:UITransitionContextToViewControllerKey]);
  EXPECT_EQ(fromViewController.view,
            [context viewForKey:UITransitionContextFromViewKey]);
  EXPECT_EQ(toViewController.view,
            [context viewForKey:UITransitionContextToViewKey]);
}

// Tests that the container view is properly returned by the context after
// initialization.
TEST_F(ContainmentTransitionContextTest, InitializationContainerView) {
  UIView* containerView = [[UIView alloc] init];
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:nil
                toViewController:nil
            parentViewController:[[UIViewController alloc] init]
                          inView:containerView
                      completion:nil];
  EXPECT_EQ(containerView, context.containerView);
}

// Tests that the view controllers receive the appropriate calls to perform a
// containment transition when prepareTransitionWithAnimator: and
// completeTransition: are called.
TEST_F(ContainmentTransitionContextTest, PrepareAndTransition) {
  TestViewController* fromViewController = [[TestViewController alloc] init];
  TestViewController* toViewController = [[TestViewController alloc] init];
  UIViewController* parentViewController = [[UIViewController alloc] init];
  [parentViewController addChildViewController:fromViewController];
  [parentViewController.view addSubview:fromViewController.view];
  [fromViewController didMoveToParentViewController:parentViewController];
  fromViewController.recordingCalls = YES;
  toViewController.recordingCalls = YES;
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:fromViewController
                toViewController:toViewController
            parentViewController:parentViewController
                          inView:nil
                      completion:nil];

  [context prepareTransitionWithAnimator:nil];

  // Check that both view controllers are children of the parent.
  EXPECT_EQ(parentViewController, fromViewController.parentViewController);
  EXPECT_EQ(parentViewController, toViewController.parentViewController);
  // Check that fromViewController received the appropriate call to prepare for
  // being removed from its parent.
  EXPECT_EQ(1U, fromViewController.willMoveToParentCount);
  EXPECT_EQ(nil, fromViewController.willMoveToParentArgument);
  // Check that the from view is still parented.
  EXPECT_NE(nil, fromViewController.view.superview);
  // Check that the to view is not yet parented.
  EXPECT_EQ(nil, toViewController.view.superview);
  // Check that toViewController didn't yet receive confirmation call for being
  // added to the parent.
  EXPECT_EQ(0U, toViewController.didMoveToParentCount);

  [context completeTransition:YES];

  // Check that only toViewController is a child of the parent.
  EXPECT_EQ(nil, fromViewController.parentViewController);
  EXPECT_EQ(parentViewController, toViewController.parentViewController);
  // Check that fromViewController didn't get more calls to prepare for being
  // removed from its parent.
  EXPECT_EQ(1U, fromViewController.willMoveToParentCount);
  // Check that the from view is not parented anymore.
  EXPECT_EQ(nil, fromViewController.view.superview);
  // Check that toViewController received the confirmation call for being added
  // to the parent.
  EXPECT_EQ(1U, toViewController.didMoveToParentCount);
  EXPECT_EQ(parentViewController, toViewController.didMoveToParentArgument);
}

TEST_F(ContainmentTransitionContextTest, CompletionBlockCalledOnCompletion) {
  __block NSUInteger count = 0;
  __block BOOL didComplete = NO;
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:nil
                toViewController:nil
            parentViewController:[[UIViewController alloc] init]
                          inView:nil
                      completion:^(BOOL innerDidComplete) {
                        count++;
                        didComplete = innerDidComplete;
                      }];
  [context completeTransition:YES];
  EXPECT_EQ(1U, count);
  EXPECT_EQ(YES, didComplete);
}

TEST_F(ContainmentTransitionContextTest, CompletionBlockCalledOnCancellation) {
  __block NSUInteger count = 0;
  __block BOOL didComplete = NO;
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:nil
                toViewController:nil
            parentViewController:[[UIViewController alloc] init]
                          inView:nil
                      completion:^(BOOL innerDidComplete) {
                        count++;
                        didComplete = innerDidComplete;
                      }];
  [context completeTransition:NO];
  EXPECT_EQ(1U, count);
  EXPECT_EQ(NO, didComplete);
}

TEST_F(ContainmentTransitionContextTest, AnimationEndedCalledOnCompletion) {
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:nil
                toViewController:nil
            parentViewController:[[UIViewController alloc] init]
                          inView:nil
                      completion:nil];
  FakeAnimator* animator = [[FakeAnimator alloc] init];
  [context prepareTransitionWithAnimator:animator];
  EXPECT_EQ(0U, animator.animationEndedCount);
  EXPECT_FALSE(animator.animationEndedArgument);

  [context completeTransition:YES];

  // Check that animation ended was called as expected.
  EXPECT_EQ(1U, animator.animationEndedCount);
  EXPECT_TRUE(animator.animationEndedArgument);

  // Check that the animator is never asked its duration or to animate. It's not
  // the context job to call them.
  EXPECT_FALSE(animator.transitionDurationCalled);
  EXPECT_FALSE(animator.animateTransitionCalled);
}

TEST_F(ContainmentTransitionContextTest, AnimationEndedCalledOnCancellation) {
  ContainmentTransitionContext* context = [[ContainmentTransitionContext alloc]
      initWithFromViewController:nil
                toViewController:nil
            parentViewController:[[UIViewController alloc] init]
                          inView:nil
                      completion:nil];
  FakeAnimator* animator = [[FakeAnimator alloc] init];
  [context prepareTransitionWithAnimator:animator];
  EXPECT_EQ(0U, animator.animationEndedCount);
  EXPECT_FALSE(animator.animationEndedArgument);

  [context completeTransition:NO];

  // Check that animation ended was called as expected.
  EXPECT_EQ(1U, animator.animationEndedCount);
  EXPECT_FALSE(animator.animationEndedArgument);

  // Check that the animator is never asked its duration or to animate. It's not
  // the context job to call them.
  EXPECT_FALSE(animator.transitionDurationCalled);
  EXPECT_FALSE(animator.animateTransitionCalled);
}
