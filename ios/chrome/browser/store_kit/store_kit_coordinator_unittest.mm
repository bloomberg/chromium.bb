// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"

#import <StoreKit/StoreKit.h>

#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/wait_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;
using testing::kWaitForUIElementTimeout;

// Test fixture for StoreKitCoordinator class.
class StoreKitCoordinatorTest : public PlatformTest {
 protected:
  StoreKitCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        coordinator_([[StoreKitCoordinator alloc]
            initWithBaseViewController:base_view_controller_]) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  UIViewController* base_view_controller_;
  StoreKitCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
};

// Tests that StoreKitCoordinator presents SKStoreProductViewController when
// openAppStoreWithParameters is called.
TEST_F(StoreKitCoordinatorTest, OpenStoreWithParamsPresentViewController) {
  NSDictionary* product_params = @{
    SKStoreProductParameterITunesItemIdentifier : @"TestITunesItemIdentifier",
    SKStoreProductParameterAffiliateToken : @"TestToken"
  };
  [coordinator_ openAppStoreWithParameters:product_params];
  EXPECT_NSEQ(product_params, coordinator_.iTunesProductParameters);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [SKStoreProductViewController class];
  }));

  [coordinator_ stop];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Tests that StoreKitCoordinator presents SKStoreProductViewController when
// openAppStore is called.
TEST_F(StoreKitCoordinatorTest, OpenStorePresentViewController) {
  NSString* kTestITunesItemIdentifier = @"TestITunesItemIdentifier";
  NSDictionary* product_params = @{
    SKStoreProductParameterITunesItemIdentifier : kTestITunesItemIdentifier,
  };
  [coordinator_ openAppStore:kTestITunesItemIdentifier];
  EXPECT_NSEQ(product_params, coordinator_.iTunesProductParameters);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [SKStoreProductViewController class];
  }));

  [coordinator_ stop];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}
