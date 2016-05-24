// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Fixture to test ContextMenuCoordinator.
class ContextMenuCoordinatorTest : public PlatformTest {
 public:
  ContextMenuCoordinatorTest() {
    window_.reset(
        [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
    [window_ makeKeyAndVisible];
    view_controller_.reset([[UIViewController alloc] init]);
    [window_ setRootViewController:view_controller_];
  }

 protected:
  base::scoped_nsobject<ContextMenuCoordinator> menu_coordinator_;
  base::scoped_nsobject<UIWindow> window_;
  base::scoped_nsobject<UIViewController> view_controller_;
};

// Tests the context menu reports as visible after presenting.
TEST_F(ContextMenuCoordinatorTest, ValidateIsVisible) {
  web::ContextMenuParams params;
  params.location = CGPointZero;
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);
  [menu_coordinator_ start];

  EXPECT_TRUE([menu_coordinator_ isVisible]);
}

// Tests the context menu dismissal.
TEST_F(ContextMenuCoordinatorTest, ValidateDismissalOnStop) {
  web::ContextMenuParams params;
  params.location = CGPointZero;
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);
  [menu_coordinator_ start];

  [menu_coordinator_ stop];

  EXPECT_FALSE([menu_coordinator_ isVisible]);
}

// Tests the context menu dismissal.
TEST_F(ContextMenuCoordinatorTest, ValidateDismissalOnDestroy) {
  web::ContextMenuParams params;
  params.location = CGPointZero;
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);
  [menu_coordinator_ start];

  menu_coordinator_.reset();

  EXPECT_FALSE([menu_coordinator_ isVisible]);
}

// Tests that only the expected actions are present on the context menu.
TEST_F(ContextMenuCoordinatorTest, ValidateActions) {
  web::ContextMenuParams params;
  params.location = CGPointZero;
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);

  NSArray* menu_titles = @[ @"foo", @"bar" ];
  for (NSString* title in menu_titles) {
    [menu_coordinator_ addItemWithTitle:title
                                 action:^{
                                 }];
  }

  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  base::scoped_nsobject<NSMutableArray> remaining_titles(
      [menu_titles mutableCopy]);
  for (UIAlertAction* action in alert_controller.actions) {
    if (action.style != UIAlertActionStyleCancel) {
      EXPECT_TRUE([remaining_titles containsObject:action.title]);
      [remaining_titles removeObject:action.title];
    }
  }

  EXPECT_EQ(0LU, [remaining_titles count]);
}

// Validates that the cancel action is present on the context menu.
TEST_F(ContextMenuCoordinatorTest, CancelButtonExists) {
  web::ContextMenuParams params;
  params.location = CGPointZero;
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);

  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  EXPECT_EQ(1LU, alert_controller.actions.count);
  EXPECT_EQ(UIAlertActionStyleCancel,
            [alert_controller.actions.firstObject style]);
}

// Tests that the ContextMenuParams are used to display context menu.
TEST_F(ContextMenuCoordinatorTest, ValidateContextMenuParams) {
  CGPoint location = CGPointMake(100.0, 125.0);
  NSString* title = @"Context Menu Title";

  web::ContextMenuParams params;
  params.location = location;
  params.menu_title.reset(title);
  params.view.reset([[view_controller_ view] retain]);
  menu_coordinator_.reset([[ContextMenuCoordinator alloc]
      initWithViewController:view_controller_
                      params:params]);
  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  EXPECT_EQ(title, alert_controller.title);

  // Only validate the popover location if it is displayed in a popover.
  if (alert_controller.popoverPresentationController) {
    CGPoint presentedLocation =
        alert_controller.popoverPresentationController.sourceRect.origin;
    EXPECT_EQ(location.x, presentedLocation.x);
    EXPECT_EQ(location.y, presentedLocation.y);
  }
}
