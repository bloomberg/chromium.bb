// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/shared/chrome/browser/ui/tab/tab_test_util.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class WebCoordinatorTest : public PlatformTest {
 public:
  WebCoordinatorTest() {
    auto navigation_manager = base::MakeUnique<TabNavigationManager>();
    navigation_manager->SetItemCount(0);
    test_web_state_.SetView([[UIView alloc] init]);
    test_web_state_.SetNavigationManager(std::move(navigation_manager));

    coordinator_ = [[WebCoordinator alloc] init];
  }

 protected:
  WebCoordinator* coordinator_;
  web::TestWebState test_web_state_;
};

// Tests that starting without the webstate still creates a view controller.
TEST_F(WebCoordinatorTest, TestStartWithoutWebState) {
  EXPECT_EQ(coordinator_.viewController, nil);
  [coordinator_ start];
  EXPECT_NE(coordinator_.viewController, nil);
}

// Tests that starting with the webstate puts the webstate's view in the VC.
TEST_F(WebCoordinatorTest, TestStartWithWebState) {
  coordinator_.webState = &test_web_state_;
  [coordinator_ start];
  EXPECT_TRUE([test_web_state_.GetView()
      isDescendantOfView:coordinator_.viewController.view]);
}

// Tests that starting and then setting the webstate puts the webstate's view in
// the VC.
TEST_F(WebCoordinatorTest, TestStartThenSetWebState) {
  [coordinator_ start];
  EXPECT_FALSE([test_web_state_.GetView()
      isDescendantOfView:coordinator_.viewController.view]);
  coordinator_.webState = &test_web_state_;
  EXPECT_TRUE([test_web_state_.GetView()
      isDescendantOfView:coordinator_.viewController.view]);
}

}  // namespace
