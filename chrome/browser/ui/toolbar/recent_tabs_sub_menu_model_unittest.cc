// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This copies parts of MenuModelTest::Delegate and combines them with the
// RecentTabsSubMenuModel since RecentTabsSubMenuModel is a
// SimpleMenuModel::Delegate and not just derived from SimpleMenuModel.
class TestRecentTabsSubMenuModel : public RecentTabsSubMenuModel {
 public:
  TestRecentTabsSubMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser,
                             browser_sync::SessionModelAssociator* associator,
                             bool can_restore_tab)
      : RecentTabsSubMenuModel(provider, browser, associator),
        can_restore_tab_(can_restore_tab),
        execute_count_(0),
        enable_count_(0) {
  }

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    bool val = command_id == IDC_RESTORE_TAB ? can_restore_tab_ :
        RecentTabsSubMenuModel::IsCommandIdEnabled(command_id);
    if (val)
      ++enable_count_;
    return val;
  }

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    ++execute_count_;
  }

  bool can_restore_tab_;
  int execute_count_;
  int mutable enable_count_;  // Mutable because IsCommandIdEnabledAt is const.
};

}  // namespace

class RecentTabsSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
   RecentTabsSubMenuModelTest()
       : sync_service_(&testing_profile_),
         associator_(&sync_service_, true) {
    associator_.SetCurrentMachineTagForTesting("RecentTabsSubMenuModelTest");
  }

 private:
  TestingProfile testing_profile_;
  testing::NiceMock<ProfileSyncServiceMock> sync_service_;

 protected:
  browser_sync::SessionModelAssociator associator_;
};

// Test disabled "Reopen closed tab" with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  TestRecentTabsSubMenuModel model(NULL, browser(), NULL, false);

  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(0, model.enable_count_);
}

// Test enabled "Reopen closed tab" with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, ReopenClosedTab) {
  TestRecentTabsSubMenuModel model(NULL, browser(), NULL, true);
  // Menu contents are identical to NoTabs test.
  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(1, model.enable_count_);
  EXPECT_EQ(1, model.execute_count_);
}

// Test enabled "Reopen closed tab" with multiple sessions, multiple windows,
// and multiple enabled tabs from other devices.
TEST_F(RecentTabsSubMenuModelTest, OtherDevices) {
  // Tabs are populated in decreasing timestamp.
  base::Time timestamp = base::Time::Now();
  const base::TimeDelta time_delta = base::TimeDelta::FromMinutes(10);

  RecentTabsBuilderTestHelper recent_tabs_builder;

  // Create 1st session : 1 window, 3 tabs
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  for (int i = 0; i < 3; ++i) {
    timestamp -= time_delta;
    recent_tabs_builder.AddTabWithInfo(0, 0, timestamp, string16());
  }

  // Create 2nd session : 2 windows, 1 tab in 1st window, 2 tabs in 2nd window
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(1);
  recent_tabs_builder.AddWindow(1);
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 0, timestamp, string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, string16());

  recent_tabs_builder.RegisterRecentTabs(&associator_);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - first inserted tab is most recent and hence is top
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3-5         <3 tabs of the only window of session 0>
  // 6           <separator>
  // 7           <section header for 2nd session>
  // 8           <the only tab of window 0 of session 1>
  // 9-10        <2 tabs of window 1 of session 2>
  // 11          <separator>
  // 12          Devices and history...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(13, num_items);
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  model.ActivatedAt(4);
  EXPECT_TRUE(model.IsEnabledAt(4));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(8);
  EXPECT_TRUE(model.IsEnabledAt(8));
  model.ActivatedAt(9);
  EXPECT_TRUE(model.IsEnabledAt(9));
  model.ActivatedAt(10);
  EXPECT_TRUE(model.IsEnabledAt(10));
  EXPECT_TRUE(model.IsEnabledAt(12));
  EXPECT_EQ(8, model.enable_count_);
  EXPECT_EQ(7, model.execute_count_);
}

TEST_F(RecentTabsSubMenuModelTest, MaxSessionsAndRecency) {
  // Create 4 sessions : each session has 1 window with 1 tab each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  for (int s = 0; s < 4; ++s) {
    recent_tabs_builder.AddSession();
    recent_tabs_builder.AddWindow(s);
    recent_tabs_builder.AddTab(s, 0);
  }
  recent_tabs_builder.RegisterRecentTabs(&associator_);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max sessions is 3, so only 3 most-recent sessions will show.
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3           <the only tab of the only window of session 3>
  // 4           <separator>
  // 5           <section header for 2nd session>
  // 6           <the only tab of the only window of session 2>
  // 7           <separator>
  // 8           <section header for 3rd session>
  // 9           <the only tab of the only window of session 1>
  // 10          <separator>
  // 11          Devices and history...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(12, num_items);

  std::vector<string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  EXPECT_EQ(tab_titles[0], model.GetLabelAt(3));
  EXPECT_EQ(tab_titles[1], model.GetLabelAt(6));
  EXPECT_EQ(tab_titles[2], model.GetLabelAt(9));
}

TEST_F(RecentTabsSubMenuModelTest, MaxTabsPerSessionAndRecency) {
  // Create a session: 2 windows with 5 tabs each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  for (int w = 0; w < 2; ++w) {
    recent_tabs_builder.AddWindow(0);
    for (int t = 0; t < 5; ++t)
      recent_tabs_builder.AddTab(0, w);
  }
  recent_tabs_builder.RegisterRecentTabs(&associator_);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max tabs per session is 4, so only 4 most-recent tabs will show,
  //   independent of which window they came from.
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for session>
  // 3-6         <4 most-recent tabs of session>
  // 7           <separator>
  // 8           Devices and history...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(9, num_items);

  std::vector<string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(tab_titles[i], model.GetLabelAt(i + 3));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidth) {
  // Create 1 session with 1 window and 1 tab.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  recent_tabs_builder.RegisterRecentTabs(&associator_);

  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3           <the only tab of the only window of session 1>
  // 4           <separator>
  // 5           Devices and history...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_, true);
  EXPECT_EQ(6, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(0));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(1));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidthNoDevices) {
  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           No tabs from other Devices

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL, false);
  EXPECT_EQ(3, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(0));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(1));
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
}
