// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/core/persistent_tab_restore_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_driver/glue/synced_session.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "components/sync_driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "grit/generated_resources.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_error_factory_mock.h"
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
                             sync_driver::OpenTabsUIDelegate* delegate)
      : RecentTabsSubMenuModel(provider, browser, delegate),
        execute_count_(0),
        enable_count_(0) {}

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    bool val = RecentTabsSubMenuModel::IsCommandIdEnabled(command_id);
    if (val)
      ++enable_count_;
    return val;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
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

  ~TestRecentTabsMenuModelDelegate() override {
    model_->SetMenuModelDelegate(NULL);
  }

  // ui::MenuModelDelegate implementation:

  void OnIconChanged(int index) override {}

  void OnMenuStructureChanged() override { got_changes_ = true; }

  bool got_changes() const { return got_changes_; }

 private:
  ui::MenuModel* model_;
  bool got_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestRecentTabsMenuModelDelegate);
};

class DummyRouter : public browser_sync::LocalSessionEventRouter {
 public:
  ~DummyRouter() override {}
  void StartRoutingTo(
      browser_sync::LocalSessionEventHandler* handler) override {}
  void Stop() override {}
};

}  // namespace

class RecentTabsSubMenuModelTest
    : public BrowserWithTestWindowTest {
 public:
  RecentTabsSubMenuModelTest()
      : sync_service_(&testing_profile_),
        local_device_(new sync_driver::LocalDeviceInfoProviderMock(
                      "RecentTabsSubMenuModelTest",
                      "Test Machine",
                      "Chromium 10k",
                      "Chrome 10k",
                      sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                      "device_id")) {
    manager_.reset(new browser_sync::SessionsSyncManager(
        sync_service_.GetSyncClient()->GetSyncSessionsClient(),
        &testing_profile_, local_device_.get(),
        scoped_ptr<browser_sync::LocalSessionEventRouter>(new DummyRouter())));
    manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS,
        syncer::SyncDataList(),
        scoped_ptr<syncer::SyncChangeProcessor>(
          new syncer::FakeSyncChangeProcessor),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock));
  }

  void WaitForLoadFromLastSession() {
    content::RunAllBlockingPoolTasksUntilIdle();
  }

  static scoped_ptr<KeyedService> GetTabRestoreService(
      content::BrowserContext* browser_context) {
    return make_scoped_ptr(new sessions::PersistentTabRestoreService(
        make_scoped_ptr(new ChromeTabRestoreServiceClient(
            Profile::FromBrowserContext(browser_context))),
        nullptr));
  }

  sync_driver::OpenTabsUIDelegate* GetOpenTabsDelegate() {
    return manager_.get();
  }

  void RegisterRecentTabs(RecentTabsBuilderTestHelper* helper) {
    helper->ExportToSessionsSyncManager(manager_.get());
  }

 private:
  TestingProfile testing_profile_;
  testing::NiceMock<ProfileSyncServiceMock> sync_service_;

  scoped_ptr<browser_sync::SessionsSyncManager> manager_;
  scoped_ptr<sync_driver::LocalDeviceInfoProviderMock> local_device_;
};

// Test disabled "Recently closed" header with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);

  // Expected menu:
  // Menu index  Menu items
  // ---------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(5, num_items);
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_FALSE(model.IsEnabledAt(4));
  EXPECT_EQ(0, model.enable_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
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
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <tab for http://foo/2>
  // 4           <tab for http://foo/1>
  // 5           <separator>
  // 6           No tabs from other Devices
  int num_items = model.GetItemCount();
  EXPECT_EQ(7, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_TRUE(model.IsEnabledAt(4));
  model.ActivatedAt(3);
  model.ActivatedAt(4);
  EXPECT_FALSE(model.IsEnabledAt(6));
  EXPECT_EQ(3, model.enable_count());
  EXPECT_EQ(3, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_TRUE(model.GetLabelFontListAt(2) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
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
  SessionServiceFactory::SetForTestProfile(profile(),
                                           make_scoped_ptr(session_service));
  SessionID tab_id;
  SessionID window_id;
  session_service->SetWindowType(window_id,
                                 Browser::TYPE_TABBED,
                                 SessionService::TYPE_NORMAL);
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
  content::RunAllBlockingPoolTasksUntilIdle();

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);
  TestRecentTabsMenuModelDelegate delegate(&model);
  EXPECT_FALSE(delegate.got_changes());

  // Expected menu before tabs/windows from last session are loaded:
  // Menu index  Menu items
  // ----------------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <separator>
  // 4           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(5, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(3));
  EXPECT_FALSE(model.IsEnabledAt(4));
  EXPECT_EQ(1, model.enable_count());

  // Wait for tabs from last session to be loaded.
  WaitForLoadFromLastSession();

  // Expected menu after tabs/windows from last session are loaded:
  // Menu index  Menu items
  // --------------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <window for the tab http://wnd1/tab0>
  // 4           <tab for http://wnd0/tab1>
  // 5           <tab for http://wnd0/tab0>
  // 6           <separator>
  // 7           No tabs from other Devices

  EXPECT_TRUE(delegate.got_changes());

  num_items = model.GetItemCount();
  EXPECT_EQ(8, num_items);

  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_TRUE(model.IsEnabledAt(4));
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(3);
  model.ActivatedAt(4);
  model.ActivatedAt(5);
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(6));
  EXPECT_FALSE(model.IsEnabledAt(7));
  EXPECT_EQ(5, model.enable_count());
  EXPECT_EQ(4, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_TRUE(model.GetLabelFontListAt(2) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(7));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(7, &url, &title));
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
    recent_tabs_builder.AddTabWithInfo(0, 0, timestamp, base::string16());
  }

  // Create 2nd session : 2 windows, 1 tab in 1st window, 2 tabs in 2nd window
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(1);
  recent_tabs_builder.AddWindow(1);
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 0, timestamp, base::string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, base::string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, base::string16());

  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - first inserted tab is most recent and hence is top
  // Menu index  Menu items
  // -----------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5-7         <3 tabs of the only window of session 0>
  // 8           <separator>
  // 9           <section header for 2nd session>
  // 10          <the only tab of window 0 of session 1>
  // 11-12       <2 tabs of window 1 of session 2>

  TestRecentTabsSubMenuModel model(NULL, browser(), GetOpenTabsDelegate());
  int num_items = model.GetItemCount();
  EXPECT_EQ(13, num_items);
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(1);
  EXPECT_TRUE(model.IsEnabledAt(1));
  model.ActivatedAt(2);
  EXPECT_FALSE(model.IsEnabledAt(2));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(6);
  EXPECT_TRUE(model.IsEnabledAt(6));
  model.ActivatedAt(7);
  EXPECT_TRUE(model.IsEnabledAt(7));
  model.ActivatedAt(10);
  EXPECT_TRUE(model.IsEnabledAt(10));
  model.ActivatedAt(11);
  EXPECT_TRUE(model.IsEnabledAt(11));
  model.ActivatedAt(12);
  EXPECT_TRUE(model.IsEnabledAt(12));

  EXPECT_EQ(7, model.enable_count());
  EXPECT_EQ(10, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_TRUE(model.GetLabelFontListAt(4) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(7));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(8));
  EXPECT_TRUE(model.GetLabelFontListAt(9) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(10));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(11));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(12));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(7, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(8, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(9, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(10, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(11, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(12, &url, &title));
}

TEST_F(RecentTabsSubMenuModelTest, MaxSessionsAndRecency) {
  // Create 4 sessions : each session has 1 window with 1 tab each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  for (int s = 0; s < 4; ++s) {
    recent_tabs_builder.AddSession();
    recent_tabs_builder.AddWindow(s);
    recent_tabs_builder.AddTab(s, 0);
  }
  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max sessions is 3, so only 3 most-recent sessions will show.
  // Menu index  Menu items
  // ----------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <the only tab of the only window of session 3>
  // 6           <separator>
  // 7           <section header for 2nd session>
  // 8           <the only tab of the only window of session 2>
  // 9           <separator>
  // 10          <section header for 3rd session>
  // 11          <the only tab of the only window of session 1>

  TestRecentTabsSubMenuModel model(NULL, browser(), GetOpenTabsDelegate());
  int num_items = model.GetItemCount();
  EXPECT_EQ(12, num_items);

  std::vector<base::string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  EXPECT_EQ(tab_titles[0], model.GetLabelAt(5));
  EXPECT_EQ(tab_titles[1], model.GetLabelAt(8));
  EXPECT_EQ(tab_titles[2], model.GetLabelAt(11));
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
  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max tabs per session is 4, so only 4 most-recent tabs will show,
  //   independent of which window they came from.
  // Menu index  Menu items
  // ---------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for session>
  // 5-8         <4 most-recent tabs of session>

  TestRecentTabsSubMenuModel model(NULL, browser(), GetOpenTabsDelegate());
  int num_items = model.GetItemCount();
  EXPECT_EQ(9, num_items);

  std::vector<base::string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(tab_titles[i], model.GetLabelAt(i + 5));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidth) {
  // Create 1 session with 1 window and 1 tab.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  RegisterRecentTabs(&recent_tabs_builder);

  // Menu index  Menu items
  // ----------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <the only tab of the only window of session 1>

  TestRecentTabsSubMenuModel model(NULL, browser(), GetOpenTabsDelegate());
  EXPECT_EQ(6, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(4));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(5));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidthNoDevices) {
  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed heaer (disabled)
  // 3           <separator>
  // 4           No tabs from other Devices

  TestRecentTabsSubMenuModel model(NULL, browser(), NULL);
  EXPECT_EQ(5, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(4));
}
