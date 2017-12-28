// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class LocationBarCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    base::test::ScopedTaskEnvironment task_environment;
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();

    coordinator_ = [[LocationBarCoordinator alloc] init];
    coordinator_.browserState = browser_state_.get();
  }

  LocationBarCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(LocationBarCoordinatorTest, Starts) {
  EXPECT_TRUE(coordinator_.locationBarView == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.locationBarView != nil);
}

TEST_F(LocationBarCoordinatorTest, Stops) {
  EXPECT_TRUE(coordinator_.locationBarView == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.locationBarView != nil);
  [coordinator_ stop];
  EXPECT_TRUE(coordinator_.locationBarView == nil);
}

}  // namespace
