// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SettingsRootTableViewControllerTest : public PlatformTest {
 public:
  SettingsRootTableViewController* Controller() {
    return [[SettingsRootTableViewController alloc]
        initWithTableViewStyle:UITableViewStylePlain
                   appBarStyle:ChromeTableViewControllerStyleWithAppBar];
  }

  SettingsNavigationController* NavigationController() {
    if (!chrome_browser_state_) {
      TestChromeBrowserState::Builder test_cbs_builder;
      chrome_browser_state_ = test_cbs_builder.Build();
    }
    return [[SettingsNavigationController alloc]
        initWithRootViewController:nil
                      browserState:chrome_browser_state_.get()
                          delegate:nil];
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(SettingsRootTableViewControllerTest, TestUpdateEditButton) {
  SettingsRootTableViewController* controller = Controller();

  id mockController = OCMPartialMock(controller);
  SettingsNavigationController* navigationController = NavigationController();
  OCMStub([mockController navigationController])
      .andReturn(navigationController);

  // Check that there the navigation controller's button if the table view isn't
  // edited and the controller has the default behavior for
  // |shouldShowEditButton|.
  controller.tableView.editing = NO;
  [controller updateEditButton];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
              controller.navigationItem.rightBarButtonItem.title);

  // Check that there the OK button if the table view is being edited and the
  // controller has the default behavior for |shouldShowEditButton|.
  controller.tableView.editing = YES;
  [controller updateEditButton];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
              controller.navigationItem.rightBarButtonItem.title);

  // Check that there the OK button if the table view isn't edited and the
  // controller returns YES for |shouldShowEditButton|.
  controller.tableView.editing = NO;
  OCMStub([mockController shouldShowEditButton]).andReturn(YES);
  [controller updateEditButton];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
              controller.navigationItem.rightBarButtonItem.title);
}
