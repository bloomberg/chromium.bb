// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_presenting_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/main/main_view_controller_test.h"
#import "ios/chrome/test/block_cleanup_test.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using MainPresentingViewControllerTest = MainViewControllerTest;

// Tests that setting the active view controller work and that completion
// handlers are called properly after the new view controller is made active.
TEST_F(MainPresentingViewControllerTest, SetActiveVCAndCompletionHandlers) {
  MainPresentingViewController* main_view_controller =
      [[MainPresentingViewController alloc] init];
  SetRootViewController(main_view_controller);
  CGRect windowRect = CGRectMake(0, 0, 200, 200);
  [main_view_controller view].frame = windowRect;

  UIViewController<TabSwitcher>* child_view_controller_1 =
      CreateTestTabSwitcher();
  [child_view_controller_1 view].frame = CGRectMake(0, 0, 10, 10);

  UIViewController<TabSwitcher>* child_view_controller_2 =
      CreateTestTabSwitcher();
  [child_view_controller_2 view].frame = CGRectMake(20, 20, 10, 10);

  // Tests that the completion handler is called when there is no preexisting
  // active view controller.
  __block BOOL completion_handler_was_called = false;
  [main_view_controller showTabSwitcher:child_view_controller_1
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
  [main_view_controller showTabViewController:child_view_controller_2
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
