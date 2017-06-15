// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#import "ios/shared/chrome/browser/ui/toolbar/toolbar_test_util.h"
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
    navigation_manager_ = base::MakeUnique<ToolbarTestNavigationManager>();
    web_state_.SetNavigationManager(std::move(navigation_manager_));

    // Initialize the coordinator.
    coordinator_ = [[TabCoordinator alloc] init];
    coordinator_.browser = GetBrowser();
    coordinator_.webState = &web_state_;
  }
  ~TabCoordinatorTest() override {
    // Restore the initial setting.
    [[NSUserDefaults standardUserDefaults] setObject:initial_setting_
                                              forKey:@"EnableBottomToolbar"];
  }
  void TearDown() override {
    if (coordinator_.started) {
      [coordinator_ stop];
    }
    coordinator_ = nil;
  }
  TabCoordinator* GetCoordinator() { return coordinator_; }

 protected:
  TabCoordinator* coordinator_;

 private:
  id initial_setting_;
  base::MessageLoop loop_;
  web::TestWebThread ui_thread_;
  ToolbarTestWebState web_state_;
  std::unique_ptr<ToolbarTestNavigationManager> navigation_manager_;
};

}  // namespace

TEST_F(TabCoordinatorTest, DefaultToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@""
                                            forKey:@"EnableBottomToolbar"];
  [coordinator_ start];
  EXPECT_EQ([TopToolbarTabViewController class],
            [coordinator_.viewController class]);
}

TEST_F(TabCoordinatorTest, TopToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@"Disabled"
                                            forKey:@"EnableBottomToolbar"];
  [GetCoordinator() start];
  EXPECT_EQ([TopToolbarTabViewController class],
            [GetCoordinator().viewController class]);
}

TEST_F(TabCoordinatorTest, BottomToolbar) {
  [[NSUserDefaults standardUserDefaults] setObject:@"Enabled"
                                            forKey:@"EnableBottomToolbar"];
  [GetCoordinator() start];
  EXPECT_EQ([BottomToolbarTabViewController class],
            [GetCoordinator().viewController class]);
}
