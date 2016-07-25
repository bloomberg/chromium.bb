// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#pragma mark - Fixture.

// Fixture to test AlertCoordinator.
class AlertCoordinatorTest : public PlatformTest {
 protected:
  AlertCoordinatorTest() {
    window_.reset(
        [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
    [window_ makeKeyAndVisible];
    view_controller_.reset([[UIViewController alloc] init]);
    [window_ setRootViewController:view_controller_];
  }

  void startAlertCoordinator() { [alert_coordinator_ start]; }

  UIViewController* getViewController() { return view_controller_; }

  AlertCoordinator* getAlertCoordinator(UIViewController* viewController) {
    return getAlertCoordinator(viewController, @"Test title");
  }

  AlertCoordinator* getAlertCoordinator(UIViewController* viewController,
                                        NSString* title) {
    alert_coordinator_.reset([[AlertCoordinator alloc]
        initWithBaseViewController:viewController
                             title:title]);
    return alert_coordinator_;
  }
  AlertCoordinator* getAlertCoordinator(UIViewController* viewController,
                                        NSString* title,
                                        CGRect rect,
                                        UIView* view) {
    AlertCoordinator* alertCoordinator =
        getAlertCoordinator(viewController, title);
    [alertCoordinator configureForActionSheetWithRect:rect popoverView:view];
    return alertCoordinator;
  }

  void deleteAlertCoordinator() { alert_coordinator_.reset(); }

 private:
  base::scoped_nsobject<AlertCoordinator> alert_coordinator_;
  base::scoped_nsobject<UIWindow> window_;
  base::scoped_nsobject<UIViewController> view_controller_;
};

#pragma mark - Tests.

// Tests the alert coordinator reports as visible after presenting and at least
// on button is created.
TEST_F(AlertCoordinatorTest, ValidateIsVisible) {
  // Setup.
  UIViewController* viewController = getViewController();
  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  ASSERT_FALSE(alertCoordinator.isVisible);
  ASSERT_EQ(nil, viewController.presentedViewController);

  // Action.
  startAlertCoordinator();

  // Test.
  EXPECT_TRUE(alertCoordinator.isVisible);
  EXPECT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  EXPECT_EQ(1LU, [alertCoordinator actionsCount]);
}

// Tests the alert coordinator reports as not visible after presenting on a non
// visible view.
TEST_F(AlertCoordinatorTest, ValidateIsNotVisible) {
  // Setup.
  base::scoped_nsobject<UIWindow> window(
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
  base::scoped_nsobject<UIViewController> viewController(
      [[UIViewController alloc] init]);
  [window setRootViewController:viewController];

  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  // Action.
  startAlertCoordinator();

  // Test.
  EXPECT_FALSE(alertCoordinator.isVisible);
  EXPECT_EQ(nil, [viewController presentedViewController]);
}

// Tests the alert coordinator has a correct title and message.
TEST_F(AlertCoordinatorTest, TitleAndMessage) {
  // Setup.
  UIViewController* viewController = getViewController();
  NSString* title = @"Foo test title!";
  NSString* message = @"Foo bar message.";

  AlertCoordinator* alertCoordinator =
      getAlertCoordinator(viewController, title);
  alertCoordinator.message = message;

  // Action.
  startAlertCoordinator();

  // Test.
  // Get the alert.
  EXPECT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alertController =
      base::mac::ObjCCastStrict<UIAlertController>(
          viewController.presentedViewController);

  // Test the results.
  EXPECT_EQ(title, alertController.title);
  EXPECT_EQ(message, alertController.message);
}

// Tests the alert coordinator dismissal.
TEST_F(AlertCoordinatorTest, ValidateDismissalOnStop) {
  // Setup.
  UIViewController* viewController = getViewController();
  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  startAlertCoordinator();

  ASSERT_TRUE(alertCoordinator.isVisible);
  ASSERT_NE(nil, viewController.presentedViewController);
  ASSERT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);

  id alertMock = [OCMockObject
      partialMockForObject:viewController.presentedViewController];
  [[alertMock expect] dismissViewControllerAnimated:NO completion:nil];

  // Action.
  [alertCoordinator stop];

  // Test.
  EXPECT_FALSE(alertCoordinator.isVisible);
  EXPECT_OCMOCK_VERIFY(alertMock);
}

// Tests the alert coordinator dismissal when the object is destroyed.
TEST_F(AlertCoordinatorTest, ValidateDismissalOnDestroy) {
  // Setup.
  UIViewController* viewController = getViewController();
  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  startAlertCoordinator();

  ASSERT_TRUE(alertCoordinator.isVisible);
  ASSERT_NE(nil, viewController.presentedViewController);
  ASSERT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);

  id alertMock = [OCMockObject
      partialMockForObject:viewController.presentedViewController];
  [[alertMock expect] dismissViewControllerAnimated:NO completion:nil];

  // Action.
  deleteAlertCoordinator();

  // Test.
  EXPECT_FALSE([alertCoordinator isVisible]);
  ASSERT_OCMOCK_VERIFY(alertMock);
}

// Tests that only the expected actions are present on the alert.
TEST_F(AlertCoordinatorTest, ValidateActions) {
  // Setup.
  UIViewController* viewController = getViewController();
  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  NSDictionary* actions = @{
    @"foo" : @(UIAlertActionStyleDestructive),
    @"foo2" : @(UIAlertActionStyleDestructive),
    @"bar" : @(UIAlertActionStyleDefault),
    @"bar2" : @(UIAlertActionStyleDefault),
    @"testCancel" : @(UIAlertActionStyleCancel),
  };

  base::scoped_nsobject<NSMutableDictionary> remainingActions(
      [actions mutableCopy]);

  // Action.
  for (id key in actions) {
    UIAlertActionStyle style =
        static_cast<UIAlertActionStyle>([actions[key] integerValue]);
    [alertCoordinator addItemWithTitle:key action:nil style:style];
  }

  // Test.
  // Present the alert.
  startAlertCoordinator();

  // Get the alert.
  EXPECT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alertController =
      base::mac::ObjCCastStrict<UIAlertController>(
          viewController.presentedViewController);

  // Test the results.
  for (UIAlertAction* action in alertController.actions) {
    EXPECT_TRUE([remainingActions objectForKey:action.title]);
    UIAlertActionStyle style =
        static_cast<UIAlertActionStyle>([actions[action.title] integerValue]);
    EXPECT_EQ(style, action.style);
    [remainingActions removeObjectForKey:action.title];
  }

  EXPECT_EQ(0LU, [remainingActions count]);
}

// Tests that only the first cancel action is added.
TEST_F(AlertCoordinatorTest, OnlyOneCancelAction) {
  // Setup.
  UIViewController* viewController = getViewController();
  AlertCoordinator* alertCoordinator = getAlertCoordinator(viewController);

  NSString* firstButtonTitle = @"Cancel1";

  // Action.
  [alertCoordinator addItemWithTitle:firstButtonTitle
                              action:nil
                               style:UIAlertActionStyleCancel];
  [alertCoordinator addItemWithTitle:@"Cancel2"
                              action:nil
                               style:UIAlertActionStyleCancel];

  // Test.
  // Present the alert.
  startAlertCoordinator();

  // Get the alert.
  EXPECT_TRUE([viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alertController =
      base::mac::ObjCCastStrict<UIAlertController>(
          viewController.presentedViewController);

  // Test the results.
  EXPECT_EQ(1LU, alertController.actions.count);

  UIAlertAction* action = [alertController.actions objectAtIndex:0];
  EXPECT_EQ(firstButtonTitle, action.title);
  EXPECT_EQ(UIAlertActionStyleCancel, action.style);
}

// Tests that if there is a popover, it uses the CGRect passed in init.
TEST_F(AlertCoordinatorTest, CGRectUsage) {
  // Setup.
  UIViewController* viewController = getViewController();
  UIView* view = [[[UIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]]
      autorelease];
  [viewController.view addSubview:view];
  CGRect rect = CGRectMake(124, 432, 126, 63);
  getAlertCoordinator(viewController, @"Title", rect, view);

  // Action.
  startAlertCoordinator();

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
}
