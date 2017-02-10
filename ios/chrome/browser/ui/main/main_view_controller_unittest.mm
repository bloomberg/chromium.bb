// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/main/main_view_controller.h"

#import <UIKit/UIKit.h>

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

TEST(MainViewControllerTest, ActiveVC) {
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

TEST(MainViewControllerTest, SetActiveVC) {
  MainViewController* main_view_controller = [[MainViewController alloc] init];
  CGRect windowRect = CGRectMake(0, 0, 200, 200);
  [main_view_controller view].frame = windowRect;

  MockViewController* child_view_controller_1 =
      [[MockViewController alloc] init];
  [child_view_controller_1 view].frame = CGRectMake(0, 0, 10, 10);

  [main_view_controller setActiveViewController:child_view_controller_1];
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
  [main_view_controller setActiveViewController:child_view_controller_2];
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

TEST(MainViewControllerTest, StatusBar) {
  MainViewController* main_view_controller = [[MainViewController alloc] init];
  // MVC needs to be the root view controller for this to work, so save the
  // current one and restore it at the end of the test.
  UIViewController* root_view_controller =
      [[[UIApplication sharedApplication] keyWindow] rootViewController];
  [[UIApplication sharedApplication] keyWindow].rootViewController =
      main_view_controller;
  UIViewController* status_bar_visible_view_controller =
      [[UIViewController alloc] init];
  [main_view_controller
      setActiveViewController:status_bar_visible_view_controller];
  // Check if the status bar is hidden by testing if the status bar rect is
  // CGRectZero.
  EXPECT_FALSE(CGRectEqualToRect(
      [UIApplication sharedApplication].statusBarFrame, CGRectZero));
  HiddenStatusBarViewController* status_bar_hidden_view_controller =
      [[HiddenStatusBarViewController alloc] init];
  [main_view_controller
      setActiveViewController:status_bar_hidden_view_controller];
  EXPECT_EQ([main_view_controller childViewControllerForStatusBarHidden],
            status_bar_hidden_view_controller);
  EXPECT_TRUE(CGRectEqualToRect(
      [UIApplication sharedApplication].statusBarFrame, CGRectZero));
  [[UIApplication sharedApplication] keyWindow].rootViewController =
      root_view_controller;
}

}  // namespace
