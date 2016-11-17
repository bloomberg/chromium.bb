// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "testing/platform_test.h"

// Tests that if there is a popover, it uses the CGRect passed in init.
TEST(ActionSheetCoordinatorTest, CGRectUsage) {
  // Setup.
  UIWindow* window = [[[UIWindow alloc]
      initWithFrame:[[UIScreen mainScreen] bounds]] autorelease];
  [window makeKeyAndVisible];
  UIViewController* viewController =
      [[[UIViewController alloc] init] autorelease];
  [window setRootViewController:viewController];

  UIView* view =
      [[[UIView alloc] initWithFrame:viewController.view.bounds] autorelease];

  [viewController.view addSubview:view];
  CGRect rect = CGRectMake(124, 432, 126, 63);
  AlertCoordinator* alertCoordinator = [[[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                           title:@"title"
                         message:nil
                            rect:rect
                            view:view] autorelease];

  // Action.
  [alertCoordinator start];

  // Test.
  // Get the alert.
  EXPECT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alertController =
      base::mac::ObjCCastStrict<UIAlertController>(
          viewController.presentedViewController);

  // Test the results.
  EXPECT_EQ(UIAlertControllerStyleActionSheet, alertController.preferredStyle);

  if (alertController.popoverPresentationController) {
    UIPopoverPresentationController* popover =
        alertController.popoverPresentationController;
    EXPECT_TRUE(CGRectEqualToRect(rect, popover.sourceRect));
    EXPECT_EQ(view, popover.sourceView);
  }

  [alertCoordinator stop];
}
