// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/main/main_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/ui/main/main_feature_flags.h"
#import "ios/chrome/test/block_cleanup_test.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HiddenStatusBarViewController : UIViewController
@end

@implementation HiddenStatusBarViewController
- (BOOL)prefersStatusBarHidden {
  return YES;
}
@end

@interface MockViewController : UIViewController
@property(nonatomic, readonly) NSInteger didMoveToParentViewControllerCount;
@property(nonatomic, weak, readonly) id didMoveToParentViewControllerArgument;
@property(nonatomic, readonly) NSInteger willMoveToParentViewControllerCount;
@property(nonatomic, weak, readonly) id willMoveToParentViewControllerArgument;
@end

@implementation MockViewController
@synthesize didMoveToParentViewControllerCount =
    _didMoveToParentViewControllerCount;
@synthesize didMoveToParentViewControllerArgument =
    _didMoveToParentViewControllerArgument;
@synthesize willMoveToParentViewControllerCount =
    _willMoveToParentViewControllerCount;
@synthesize willMoveToParentViewControllerArgument =
    _willMoveToParentViewControllerArgument;

- (void)didMoveToParentViewController:(UIViewController*)parent {
  _didMoveToParentViewControllerCount++;
  _didMoveToParentViewControllerArgument = parent;
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  _willMoveToParentViewControllerCount++;
  _willMoveToParentViewControllerArgument = parent;
}

@end

namespace {

class MainViewControllerTest : public BlockCleanupTest {
 public:
  MainViewControllerTest() = default;
  ~MainViewControllerTest() override = default;

  void TearDown() override {
    if (original_root_view_controller_) {
      [[UIApplication sharedApplication] keyWindow].rootViewController =
          original_root_view_controller_;
      original_root_view_controller_ = nil;
    }
  }

  // Sets the current key window's rootViewController and saves a pointer to
  // the original VC to allow restoring it at the end of the test.
  void SetRootViewController(UIViewController* new_root_view_controller) {
    original_root_view_controller_ =
        [[[UIApplication sharedApplication] keyWindow] rootViewController];
    [[UIApplication sharedApplication] keyWindow].rootViewController =
        new_root_view_controller;
  }

 private:
  // The key window's original root view controller, which must be restored at
  // the end of the test.
  UIViewController* original_root_view_controller_;

  DISALLOW_COPY_AND_ASSIGN(MainViewControllerTest);
};

TEST_F(MainViewControllerTest, ActiveVC) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(kTabSwitcherPresentsBVC);

  MainViewController* main_view_controller = [[MainViewController alloc] init];
  UIViewController* child_view_controller_1 = [[UIViewController alloc] init];
  UIViewController* child_view_controller_2 = [[UIViewController alloc] init];

  // Test that the active view controller is always the first child view
  // controller.
  EXPECT_EQ(nil, [main_view_controller activeViewController]);
  [main_view_controller addChildViewController:child_view_controller_1];
  EXPECT_EQ(child_view_controller_1,
            [main_view_controller activeViewController]);
  [main_view_controller addChildViewController:child_view_controller_2];
  EXPECT_EQ(child_view_controller_1,
            [main_view_controller activeViewController]);
  [child_view_controller_1 removeFromParentViewController];
  EXPECT_EQ(child_view_controller_2,
            [main_view_controller activeViewController]);
}

TEST_F(MainViewControllerTest, SetActiveVCWithContainment) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(kTabSwitcherPresentsBVC);

  MainViewController* main_view_controller = [[MainViewController alloc] init];
  CGRect windowRect = CGRectMake(0, 0, 200, 200);
  [main_view_controller view].frame = windowRect;

  MockViewController* child_view_controller_1 =
      [[MockViewController alloc] init];
  [child_view_controller_1 view].frame = CGRectMake(0, 0, 10, 10);

  [main_view_controller setActiveViewController:child_view_controller_1
                                     completion:nil];
  EXPECT_EQ(child_view_controller_1,
            [[main_view_controller childViewControllers] firstObject]);
  EXPECT_EQ([child_view_controller_1 view],
            [[main_view_controller view].subviews firstObject]);
  EXPECT_NSEQ(NSStringFromCGRect(windowRect),
              NSStringFromCGRect([child_view_controller_1 view].frame));
  EXPECT_EQ(1, [child_view_controller_1 didMoveToParentViewControllerCount]);
  EXPECT_EQ(main_view_controller,
            [child_view_controller_1 didMoveToParentViewControllerArgument]);
  // Expect there to have also been an automatic call to
  // -willMoveToParentViewController.
  EXPECT_EQ(1, [child_view_controller_1 willMoveToParentViewControllerCount]);
  EXPECT_EQ(main_view_controller,
            [child_view_controller_1 willMoveToParentViewControllerArgument]);

  UIViewController* child_view_controller_2 = [[UIViewController alloc] init];
  [main_view_controller setActiveViewController:child_view_controller_2
                                     completion:nil];
  EXPECT_EQ(child_view_controller_2,
            [[main_view_controller childViewControllers] firstObject]);
  EXPECT_EQ(1U, [[main_view_controller childViewControllers] count]);
  EXPECT_EQ(nil, [[child_view_controller_1 view] superview]);
  EXPECT_EQ(2, [child_view_controller_1 willMoveToParentViewControllerCount]);
  EXPECT_EQ(nil,
            [child_view_controller_1 willMoveToParentViewControllerArgument]);
  // Expect there to have also been an automatic call to
  // -didMoveToParentViewController.
  EXPECT_EQ(2, [child_view_controller_1 willMoveToParentViewControllerCount]);
  EXPECT_EQ(nil,
            [child_view_controller_1 didMoveToParentViewControllerArgument]);
}

TEST_F(MainViewControllerTest, StatusBar) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(kTabSwitcherPresentsBVC);

  MainViewController* main_view_controller = [[MainViewController alloc] init];
  SetRootViewController(main_view_controller);

  UIViewController* status_bar_visible_view_controller =
      [[UIViewController alloc] init];
  [main_view_controller
      setActiveViewController:status_bar_visible_view_controller
                   completion:nil];
  // Check if the status bar is hidden by testing if the status bar rect is
  // CGRectZero.
  EXPECT_FALSE(CGRectEqualToRect(
      [UIApplication sharedApplication].statusBarFrame, CGRectZero));
  HiddenStatusBarViewController* status_bar_hidden_view_controller =
      [[HiddenStatusBarViewController alloc] init];
  [main_view_controller
      setActiveViewController:status_bar_hidden_view_controller
                   completion:nil];
  EXPECT_EQ([main_view_controller childViewControllerForStatusBarHidden],
            status_bar_hidden_view_controller);
  EXPECT_TRUE(CGRectEqualToRect(
      [UIApplication sharedApplication].statusBarFrame, CGRectZero));
}

// Tests that completion handlers are called properly after the new view
// controller is made active, when using containment
TEST_F(MainViewControllerTest, CompletionHandlersWithContainment) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(kTabSwitcherPresentsBVC);

  MainViewController* main_view_controller = [[MainViewController alloc] init];
  CGRect windowRect = CGRectMake(0, 0, 200, 200);
  [main_view_controller view].frame = windowRect;

  UIViewController* child_view_controller_1 = [[UIViewController alloc] init];
  [child_view_controller_1 view].frame = CGRectMake(0, 0, 10, 10);

  UIViewController* child_view_controller_2 = [[UIViewController alloc] init];
  [child_view_controller_2 view].frame = CGRectMake(20, 20, 10, 10);

  // Tests that the completion handler is called when there is no preexisting
  // active view controller.
  __block BOOL completion_handler_was_called = false;
  [main_view_controller setActiveViewController:child_view_controller_1
                                     completion:^{
                                       completion_handler_was_called = YES;
                                     }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);

  // Tests that the completion handler is called when replacing an existing
  // active view controller.
  completion_handler_was_called = NO;
  [main_view_controller setActiveViewController:child_view_controller_2
                                     completion:^{
                                       completion_handler_was_called = YES;
                                     }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
}

// Tests that setting the active view controller work and that completion
// handlers are called properly after the new view controller is made active,
// when using presentation.
TEST_F(MainViewControllerTest,
       SetActiveVCAndCompletionHandlersWithPresentation) {
  base::test::ScopedFeatureList enable_feature;
  enable_feature.InitAndEnableFeature(kTabSwitcherPresentsBVC);

  MainViewController* main_view_controller = [[MainViewController alloc] init];
  SetRootViewController(main_view_controller);
  CGRect windowRect = CGRectMake(0, 0, 200, 200);
  [main_view_controller view].frame = windowRect;

  UIViewController* child_view_controller_1 = [[UIViewController alloc] init];
  [child_view_controller_1 view].frame = CGRectMake(0, 0, 10, 10);

  UIViewController* child_view_controller_2 = [[UIViewController alloc] init];
  [child_view_controller_2 view].frame = CGRectMake(20, 20, 10, 10);

  // Tests that the completion handler is called when there is no preexisting
  // active view controller.
  __block BOOL completion_handler_was_called = false;
  [main_view_controller setActiveViewController:child_view_controller_1
                                     completion:^{
                                       completion_handler_was_called = YES;
                                     }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_EQ(main_view_controller.presentedViewController,
            child_view_controller_1);

  // Tests that the completion handler is called when replacing an existing
  // active view controller.
  completion_handler_was_called = NO;
  [main_view_controller setActiveViewController:child_view_controller_2
                                     completion:^{
                                       completion_handler_was_called = YES;
                                     }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_EQ(main_view_controller.presentedViewController,
            child_view_controller_2);
}

}  // namespace
