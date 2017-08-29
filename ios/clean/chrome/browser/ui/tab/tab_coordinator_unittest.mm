// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#include "ios/web/public/test/test_web_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TabCoordinatorTest : public BrowserCoordinatorTest {
 protected:
  TabCoordinatorTest()
      : loop_(base::MessageLoop::TYPE_IO),
        ui_thread_(web::WebThread::UI, &loop_) {
    // Store the initial setting.
    initial_setting_ = [[NSUserDefaults standardUserDefaults]
        objectForKey:@"EnableBottomToolbar"];

    // Initialize the web state.
    auto navigation_manager = base::MakeUnique<web::TestNavigationManager>();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    // Initialize the coordinator.
    coordinator_ = [[TabCoordinator alloc] init];
    coordinator_.browser = GetBrowser();
    coordinator_.webState = &web_state_;
  }
  ~TabCoordinatorTest() override {
    // Restore the initial setting.
    [[NSUserDefaults standardUserDefaults] setObject:initial_setting_
                                              forKey:@"EnableBottomToolbar"];

    // Explicitly disconnect the mediator so there won't be any WebStateList
    // observers when web_state_list_ gets dealloc.
    [coordinator_ disconnect];
  }
  void TearDown() override {
    if (coordinator_.started) {
      [coordinator_ stop];
    }
    coordinator_ = nil;
  }

 protected:
  TabCoordinator* coordinator_;

 private:
  id initial_setting_;
  base::MessageLoop loop_;
  web::TestWebThread ui_thread_;
  ToolbarTestWebState web_state_;
};

}  // namespace

// TODO(crbug.com/759761): Reenable this test when threading issues are fixed
// for this test fixture.
TEST_F(TabCoordinatorTest, DISABLED_DefaultToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@""
                                            forKey:@"EnableBottomToolbar"];
  [coordinator_ start];
  EXPECT_EQ([TopToolbarTabViewController class],
            [coordinator_.viewController class]);
}

// TODO(crbug.com/759761): Reenable this test when threading issues are fixed
// for this test fixture.
TEST_F(TabCoordinatorTest, DISABLED_TopToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@"Disabled"
                                            forKey:@"EnableBottomToolbar"];
  [coordinator_ start];
  EXPECT_EQ([TopToolbarTabViewController class],
            [coordinator_.viewController class]);
}

// TODO(crbug.com/759761): Reenable this test when threading issues are fixed
// for this test fixture.
TEST_F(TabCoordinatorTest, DISABLED_BottomToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@"Enabled"
                                            forKey:@"EnableBottomToolbar"];
  [coordinator_ start];
  EXPECT_EQ([BottomToolbarTabViewController class],
            [coordinator_.viewController class]);
}
