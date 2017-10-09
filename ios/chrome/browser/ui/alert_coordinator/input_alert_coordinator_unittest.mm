// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/input_alert_coordinator.h"

#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using InputAlertCoordinatorTest = PlatformTest;

TEST_F(InputAlertCoordinatorTest, AddTextField) {
  // Setup.
  UIViewController* viewController = [[UIViewController alloc] init];
  InputAlertCoordinator* alertCoordinator =
      [[InputAlertCoordinator alloc] initWithBaseViewController:viewController
                                                          title:@"Test"
                                                        message:nil];

  void (^emptyHandler)(UITextField* textField) = ^(UITextField* textField) {
  };
  id alert =
      [OCMockObject partialMockForObject:alertCoordinator.alertController];
  [[alert expect] addTextFieldWithConfigurationHandler:emptyHandler];

  // Action.
  [alertCoordinator addTextFieldWithConfigurationHandler:emptyHandler];

  // Test.
  EXPECT_OCMOCK_VERIFY(alert);
}

TEST_F(InputAlertCoordinatorTest, GetTextFields) {
  // Setup.
  UIViewController* viewController = [[UIViewController alloc] init];
  InputAlertCoordinator* alertCoordinator =
      [[InputAlertCoordinator alloc] initWithBaseViewController:viewController
                                                          title:@"Test"
                                                        message:nil];

  NSArray<UITextField*>* array = [NSArray array];
  id alert =
      [OCMockObject partialMockForObject:alertCoordinator.alertController];
  [[[alert expect] andReturn:array] textFields];

  // Action.
  NSArray<UITextField*>* resultArray = alertCoordinator.textFields;

  // Test.
  EXPECT_EQ(array, resultArray);
  EXPECT_OCMOCK_VERIFY(alert);
}
