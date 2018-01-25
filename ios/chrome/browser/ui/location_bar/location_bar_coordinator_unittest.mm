// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class LocationBarCoordinatorTest : public PlatformTest {
 protected:
  LocationBarCoordinatorTest() : web_state_list_(&web_state_list_delegate_) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();

    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetCurrentURL(GURL("http://test/"));
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());

    coordinator_ = [[LocationBarCoordinator alloc] init];
    coordinator_.browserState = browser_state_.get();
    coordinator_.webStateList = &web_state_list_;
  }

  base::test::ScopedTaskEnvironment task_environment_;
  LocationBarCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
};

TEST_F(LocationBarCoordinatorTest, Stops) {
  EXPECT_TRUE(coordinator_.locationBarView == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.locationBarView != nil);
  [coordinator_ stop];
  EXPECT_TRUE(coordinator_.locationBarView == nil);
}

}  // namespace
