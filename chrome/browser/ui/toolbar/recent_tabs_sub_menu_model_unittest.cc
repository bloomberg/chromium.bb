// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/persistent_tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
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
                             browser_sync::SessionModelAssociator* associator)
      : RecentTabsSubMenuModel(provider, browser, associator),
        execute_count_(0),
        enable_count_(0) {
  }

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    bool val = RecentTabsSubMenuModel::IsCommandIdEnabled(command_id);
    if (val)
      ++enable_count_;
    return val;
  }

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    ++execute_count_;
  }

  int execute_count() const { return execute_count_; }
  int enable_count() const { return enable_count_; }

 private:
  int execute_count_;
  int mutable enable_count_;  // Mutable because IsCommandIdEnabledAt is const.

  DISALLOW_COPY_AND_ASSIGN(TestRecentTabsSubMenuModel);
};

class TestRecentTabsMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  explicit TestRecentTabsMenuModelDelegate(ui::MenuModel* model)
      : model_(model),
        got_changes_(false) {
    model_->SetMenuModelDelegate(this);
  }

  virtual ~TestRecentTabsMenuModelDelegate() {
    model_->SetMenuModelDelegate(NULL);
  }

  // ui::MenuModelDelegate implementation:

  virtual void OnIconChanged(int index) OVERRIDE {
  }

  virtual void OnMenuStructureChanged() OVERRIDE {
    got_changes_ = true;
  }

  bool got_changes() const { return got_changes_; }

 private:
  ui::MenuModel* model_;
  bool got_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestRecentTabsMenuModelDelegate);
};

}  // namespace

class RecentTabsSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
   RecentTabsSubMenuModelTest()
       : sync_service_(&testing_profile_),
         associator_(&sync_service_, true) {
    associator_.SetCurrentMachineTagForTesting("RecentTabsSubMenuModelTest");
  }

  void WaitForLoadFromLastSession() {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }

  static BrowserContextKeyedService* GetTabRestoreService(
      content::BrowserContext* browser_context) {
    // Ownership is tranfered to the profile.
    return new PersistentTabRestoreService(
        Profile::FromBrowserContext(browser_context), NULL);;
  }

 private:
  TestingProfile testing_profile_;
  testing::NiceMock<ProfileSyncServiceMock> sync_service_;

 protected:
  browser_sync::SessionModelAssociator associator_;
};

// Test disabled "Recently closed" header with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);

  // Expected menu:
  // Menu index  Menu items
  // ---------------------------------------------
  // 0           Recently closed header (disabled)
  // 1           <separator>
  // 2           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(0, model.enable_count());

  EXPECT_EQ(NULL, model.GetLabelFontAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontAt(2));

  std::string url;
  string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
}

// Test enabled "Recently closed" header with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, RecentlyClosedTabsFromCurrentSession) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(), RecentTabsSubMenuModelTest::GetTabRestoreService);

  // Add 2 tabs and close them.
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  browser()->tab_strip_model()->CloseAllTabs();

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);
  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------
  // 0           Recently closed header
  // 1           <tab for http://foo/2>
  // 2           <tab for http://foo/1>
  // 3           <separator>
  // 4           No tabs from other Devices
  int num_items = model.GetItemCount();
  EXPECT_EQ(5, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_TRUE(model.IsEnabledAt(2));
  model.ActivatedAt(1);
  model.ActivatedAt(2);
  EXPECT_FALSE(model.IsEnabledAt(4));
  EXPECT_EQ(2, model.enable_count());
  EXPECT_EQ(2, model.execute_count());

  EXPECT_TRUE(model.GetLabelFontAt(0) != NULL);
  EXPECT_EQ(NULL, model.GetLabelFontAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontAt(4));

  std::string url;
  string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
}

// TODO(sail): enable this test when dynamic model is enabled in
// RecentTabsSubMenuModel.
#if defined(OS_MACOSX)
#define MAYBE_RecentlyClosedTabsAndWindowsFromLastSession \
    DISABLED_RecentlyClosedTabsAndWindowsFromLastSession
#else
#define MAYBE_RecentlyClosedTabsAndWindowsFromLastSession \
    RecentlyClosedTabsAndWindowsFromLastSession
#endif
TEST_F(RecentTabsSubMenuModelTest,
       MAYBE_RecentlyClosedTabsAndWindowsFromLastSession) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(), RecentTabsSubMenuModelTest::GetTabRestoreService);

  // Add 2 tabs and close them.
  AddTab(browser(), GURL("http://wnd/tab0"));
  AddTab(browser(), GURL("http://wnd/tab1"));
  browser()->tab_strip_model()->CloseAllTabs();

  // Create a SessionService for the profile (profile owns the service) and add
  // a window with a tab to this session.
  SessionService* session_service = new SessionService(profile());
  SessionServiceFactory::SetForTestProfile(profile(), session_service);
  SessionID tab_id;
  SessionID window_id;
  session_service->SetWindowType(
      window_id, Browser::TYPE_TABBED, SessionService::TYPE_NORMAL);
  session_service->SetTabWindow(window_id, tab_id);
  session_service->SetTabIndexInWindow(window_id, tab_id, 0);
  session_service->SetSelectedTabInWindow(window_id, 0);
  session_service->UpdateTabNavigation(
      window_id, tab_id,
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://wnd1/tab0", "title"));
  // Set this, otherwise previous session won't be loaded.
  profile()->set_last_session_exited_cleanly(false);
  // Move this session to the last so that TabRestoreService will load it as the
  // last session.
  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  // Create a new TabRestoreService so that it'll load the recently closed tabs
  // and windows afresh.
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(), RecentTabsSubMenuModelTest::GetTabRestoreService);
  // Let the shutdown of previous TabRestoreService run.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);
  TestRecentTabsMenuModelDelegate delegate(&model);
  EXPECT_FALSE(delegate.got_changes());

  // Expected menu before tabs/windows from last session are loaded:
  // Menu index  Menu items
  // ----------------------------------------------------------------
  // 0           Recently closed header
  // 1           <separator>
  // 2           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(0, model.enable_count());

  // Wait for tabs from last session to be loaded.
  WaitForLoadFromLastSession();

  // Expected menu after tabs/windows from last session are loaded:
  // Menu index  Menu items
  // --------------------------------------------------------------
  // 0           Recently closed header
  // 1           <window for the tab http://wnd1/tab0>
  // 2           <tab for http://wnd0/tab1>
  // 3           <tab for http://wnd0/tab0>
  // 4           <separator>
  // 5           No tabs from other Devices

  EXPECT_TRUE(delegate.got_changes());

  num_items = model.GetItemCount();
  EXPECT_EQ(6, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_TRUE(model.IsEnabledAt(2));
  EXPECT_TRUE(model.IsEnabledAt(3));
  model.ActivatedAt(1);
  model.ActivatedAt(2);
  model.ActivatedAt(3);
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(4));
  EXPECT_FALSE(model.IsEnabledAt(5));
  EXPECT_EQ(3, model.enable_count());
  EXPECT_EQ(3, model.execute_count());

  EXPECT_TRUE(model.GetLabelFontAt(0) != NULL);
  EXPECT_EQ(NULL, model.GetLabelFontAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontAt(5));

  std::string url;
  string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
}

// Test disabled "Recently closed" header with multiple sessions, multiple
// windows, and multiple enabled tabs from other devices.
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
  // -----------------------------------------------------
  // 0           Recently closed header (disabled)
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3-5         <3 tabs of the only window of session 0>
  // 6           <separator>
  // 7           <section header for 2nd session>
  // 8           <the only tab of window 0 of session 1>
  // 9-10        <2 tabs of window 1 of session 2>
  // 11          <separator>
  // 12          More...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_);
  int num_items = model.GetItemCount();
  EXPECT_EQ(13, num_items);
  model.ActivatedAt(0);
  EXPECT_FALSE(model.IsEnabledAt(0));
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
  EXPECT_EQ(7, model.enable_count());
  EXPECT_EQ(7, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontAt(1));
  EXPECT_TRUE(model.GetLabelFontAt(2) != NULL);
  EXPECT_EQ(NULL, model.GetLabelFontAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontAt(6));
  EXPECT_TRUE(model.GetLabelFontAt(7) != NULL);
  EXPECT_EQ(NULL, model.GetLabelFontAt(8));
  EXPECT_EQ(NULL, model.GetLabelFontAt(9));
  EXPECT_EQ(NULL, model.GetLabelFontAt(10));
  EXPECT_EQ(NULL, model.GetLabelFontAt(11));
  EXPECT_EQ(NULL, model.GetLabelFontAt(12));

  std::string url;
  string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(7, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(8, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(9, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(10, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(11, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(12, &url, &title));
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
  // ----------------------------------------------------------
  // 0           Recently closed header (disabled)
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
  // 11          More...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_);
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
  // ---------------------------------------------
  // 0           Recently closed header (disabled)
  // 1           <separator>
  // 2           <section header for session>
  // 3-6         <4 most-recent tabs of session>
  // 7           <separator>
  // 8           More...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_);
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
  // ----------------------------------------------------------
  // 0           Recently closed header (disabled)
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3           <the only tab of the only window of session 1>
  // 4           <separator>
  // 5           More...

  TestRecentTabsSubMenuModel model(NULL, browser(), &associator_);
  EXPECT_EQ(6, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(0));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(1));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidthNoDevices) {
  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------------
  // 0           Recently closed heaer (disabled)
  // 1           <separator>
  // 2           No tabs from other Devices

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);
  EXPECT_EQ(3, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(0));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(1));
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
}
