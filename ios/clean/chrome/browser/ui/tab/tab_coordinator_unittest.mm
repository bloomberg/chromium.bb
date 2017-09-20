// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_coordinator.h"
#import "ios/clean/chrome/browser/ui/tab/tab_strip_tab_coordinator.h"

#import "base/mac/foundation_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#import "ios/clean/chrome/browser/ui/tab/tab_container_view_controller.h"
#include "ios/clean/chrome/browser/ui/tab/tab_features.h"
#import "ios/clean/chrome/browser/ui/tab/tab_strip_tab_container_view_controller.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TabCoordinatorTest : public BrowserCoordinatorTest {
 protected:
  TabCoordinatorTest() {
    // Initialize the web state.
    auto navigation_manager = base::MakeUnique<web::TestNavigationManager>();
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }
  ~TabCoordinatorTest() override {
    // Explicitly disconnect the mediator so there won't be any WebStateList
    // observers when web_state_list_ gets dealloc.
    [coordinator_ disconnect];
    [coordinator_ stop];
    coordinator_ = nil;
  }
  void SetUpDefaultCoordinator() {
    coordinator_ = [[TabCoordinator alloc] init];
    coordinator_.browser = GetBrowser();
    coordinator_.webState = &web_state_;
  }
  void SetUpTabStripCoordinator() {
    coordinator_ = [[TabStripTabCoordinator alloc] init];
    coordinator_.browser = GetBrowser();
    coordinator_.webState = &web_state_;
  }
  TabContainerViewController* TabViewController() {
    return base::mac::ObjCCastStrict<TabContainerViewController>(
        coordinator_.viewController);
  }
  void EnableBottomToolbar() {
    scoped_feature_list_.InitAndEnableFeature(kTabFeaturesBottomToolbar);
  }

 protected:
  TabCoordinator* coordinator_;
  ToolbarTestWebState web_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(TabCoordinatorTest, DefaultTabContainer) {
  SetUpDefaultCoordinator();
  [coordinator_ start];
  EXPECT_EQ([TabContainerViewController class],
            [coordinator_.viewController class]);
}

TEST_F(TabCoordinatorTest, TabStripTabContainer) {
  SetUpTabStripCoordinator();
  [coordinator_ start];
  EXPECT_EQ([TabStripTabContainerViewController class],
            [coordinator_.viewController class]);
}

TEST_F(TabCoordinatorTest, TopToolbar) {
  SetUpDefaultCoordinator();
  [coordinator_ start];
  EXPECT_EQ(NO, TabViewController().usesBottomToolbar);
}

TEST_F(TabCoordinatorTest, EnableBottomToolbar) {
  EnableBottomToolbar();
  SetUpDefaultCoordinator();
  [coordinator_ start];
  EXPECT_EQ(YES, TabViewController().usesBottomToolbar);
}
