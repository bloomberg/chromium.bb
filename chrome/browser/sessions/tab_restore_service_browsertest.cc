// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"

typedef TabRestoreService::Tab Tab;

// Create subclass that overrides TimeNow so that we can control the time used
// for closed tabs and windows.
class TabRestoreTimeFactory : public TabRestoreService::TimeFactory {
 public:
  TabRestoreTimeFactory() : time_(base::Time::Now()) {}

  virtual ~TabRestoreTimeFactory() {}

  virtual base::Time TimeNow() {
    return time_;
  }

 private:
  base::Time time_;
};

class TabRestoreServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  TabRestoreServiceTest() {
    url1_ = GURL("http://1");
    url2_ = GURL("http://2");
    url3_ = GURL("http://3");
  }

  ~TabRestoreServiceTest() {
  }

 protected:
  // testing::Test overrides
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    time_factory_ = new TabRestoreTimeFactory();
    service_.reset(new TabRestoreService(profile(), time_factory_));
    WebKit::initialize(&webkit_platform_support_);
  }

  virtual void TearDown() {
    service_.reset();
    delete time_factory_;
    ChromeRenderViewHostTestHarness::TearDown();
    WebKit::shutdown();
  }

  void AddThreeNavigations() {
    // Navigate to three URLs.
    NavigateAndCommit(url1_);
    NavigateAndCommit(url2_);
    NavigateAndCommit(url3_);
  }

  void NavigateToIndex(int index) {
    // Navigate back. We have to do this song and dance as NavigationController
    // isn't happy if you navigate immediately while going back.
    controller().GoToIndex(index);
    contents()->CommitPendingNavigation();
  }

  void RecreateService() {
    // Must set service to null first so that it is destroyed before the new
    // one is created.
    service_.reset();
    service_.reset(new TabRestoreService(profile(), time_factory_));
    service_->LoadTabsFromLastSession();
  }

  // Adds a window with one tab and url to the profile's session service.
  // If |pinned| is true, the tab is marked as pinned in the session service.
  void AddWindowWithOneTabToSessionService(bool pinned) {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile());
    SessionID tab_id;
    SessionID window_id;
    session_service->SetWindowType(window_id, Browser::TYPE_TABBED);
    session_service->SetTabWindow(window_id, tab_id);
    session_service->SetTabIndexInWindow(window_id, tab_id, 0);
    session_service->SetSelectedTabInWindow(window_id, 0);
    if (pinned)
      session_service->SetPinnedState(window_id, tab_id, true);
    NavigationEntry entry;
    entry.set_url(url1_);
    session_service->UpdateTabNavigation(window_id, tab_id, 0, entry);
  }

  // Creates a SessionService and assigns it to the Profile. The SessionService
  // is configured with a single window with a single tab pointing at url1_ by
  // way of AddWindowWithOneTabToSessionService. If |pinned| is true, the
  // tab is marked as pinned in the session service.
  void CreateSessionServiceWithOneWindow(bool pinned) {
    // The profile takes ownership of this.
    SessionService* session_service = new SessionService(profile());
    SessionServiceFactory::SetForTestProfile(profile(), session_service);

    AddWindowWithOneTabToSessionService(pinned);

    // Set this, otherwise previous session won't be loaded.
    profile()->set_last_session_exited_cleanly(false);
  }

  GURL url1_;
  GURL url2_;
  GURL url3_;
  scoped_ptr<TabRestoreService> service_;
  TabRestoreTimeFactory* time_factory_;
  content::RenderViewTest::RendererWebKitPlatformSupportImplNoSandbox
      webkit_platform_support_;
};

TEST_F(TabRestoreServiceTest, Basic) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  EXPECT_TRUE(tab->extension_app_id.empty());
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            tab->timestamp.ToInternalValue());

  NavigateToIndex(1);

  // And check again.
  service_->CreateHistoricalTab(&controller(), -1);

  // There should be two entries now.
  ASSERT_EQ(2U, service_->entries().size());

  // Make sure the entry matches
  entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(url1_, tab->navigations[0].virtual_url());
  EXPECT_EQ(url2_, tab->navigations[1].virtual_url());
  EXPECT_EQ(url3_, tab->navigations[2].virtual_url());
  EXPECT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            tab->timestamp.ToInternalValue());
}

// Make sure TabRestoreService doesn't create an entry for a tab with no
// navigations.
TEST_F(TabRestoreServiceTest, DontCreateEmptyTab) {
  service_->CreateHistoricalTab(&controller(), -1);
  EXPECT_TRUE(service_->entries().empty());
}

// Make sure TabRestoreService doesn't create an entry for print preview tab.
TEST_F(TabRestoreServiceTest, DontRestorePrintPreviewTab) {
  AddThreeNavigations();

  // Navigate to a print preview tab.
  GURL printPreviewURL(chrome::kChromeUIPrintURL);
  NavigateAndCommit(printPreviewURL);
  EXPECT_EQ(printPreviewURL, contents()->GetURL());
  EXPECT_EQ(4, controller().entry_count());

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);

  // Verify that print preview tab is not restored.
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_NE(printPreviewURL, tab->navigations[0].virtual_url());
  EXPECT_NE(printPreviewURL, tab->navigations[1].virtual_url());
  EXPECT_NE(printPreviewURL, tab->navigations[2].virtual_url());
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            tab->timestamp.ToInternalValue());
}

// Tests restoring a single tab.
TEST_F(TabRestoreServiceTest, Restore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  EXPECT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            tab->timestamp.ToInternalValue());
}

// Tests restoring a single pinned tab.
TEST_F(TabRestoreServiceTest, RestorePinnedAndApp) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // We have to explicitly mark the tab as pinned as there is no browser for
  // these tests.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  tab->pinned = true;
  const std::string extension_app_id("test");
  tab->extension_app_id = extension_app_id;

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  tab = static_cast<Tab*>(entry);
  EXPECT_TRUE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(extension_app_id == tab->extension_app_id);
}

// Make sure we persist entries to disk that have post data.
TEST_F(TabRestoreServiceTest, DontPersistPostData) {
  AddThreeNavigations();
  controller().GetEntryAtIndex(0)->set_has_post_data(true);
  controller().GetEntryAtIndex(1)->set_has_post_data(true);
  controller().GetEntryAtIndex(2)->set_has_post_data(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  const TabRestoreService::Entry* restored_entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, restored_entry->type);

  const Tab* restored_tab =
      static_cast<const Tab*>(restored_entry);
  // There should be 3 navs.
  ASSERT_EQ(3U, restored_tab->navigations.size());
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            restored_tab->timestamp.ToInternalValue());
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all the navigations have post
// data, so that nothing should be persisted.
TEST_F(TabRestoreServiceTest, DontLoadTwice) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);
  ASSERT_EQ(1U, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  service_->LoadTabsFromLastSession();

  // There should only be one entry.
  ASSERT_EQ(1U, service_->entries().size());
}

// Makes sure we load the previous session as necessary.
TEST_F(TabRestoreServiceTest, LoadPreviousSession) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  service_->LoadTabsFromLastSession();

  // Make sure we get back one entry with one tab whose url is url1.
  ASSERT_EQ(1U, service_->entries().size());
  TabRestoreService::Entry* entry2 = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry2->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry2);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->timestamp.ToInternalValue());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1U, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_EQ(0, window->tabs[0].timestamp.ToInternalValue());
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].virtual_url());
}

// Makes sure we don't attempt to load previous sessions after a restore.
TEST_F(TabRestoreServiceTest, DontLoadAfterRestore) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  service_->LoadTabsFromLastSession();

  // Because we restored a session TabRestoreService shouldn't load the tabs.
  ASSERT_EQ(0U, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions after a clean exit.
TEST_F(TabRestoreServiceTest, DontLoadAfterCleanExit) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  profile()->set_last_session_exited_cleanly(true);

  service_->LoadTabsFromLastSession();

  ASSERT_EQ(0U, service_->entries().size());
}

TEST_F(TabRestoreServiceTest, LoadPreviousSessionAndTabs) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(&controller(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToInternalValue());
  ASSERT_EQ(1U, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_EQ(0, window->tabs[0].timestamp.ToInternalValue());
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].virtual_url());

  // Then the closed tab.
  entry = *(++service_->entries().begin());
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_EQ(time_factory_->TimeNow().ToInternalValue(),
            tab->timestamp.ToInternalValue());
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Make sure pinned state is correctly loaded from session service.
TEST_F(TabRestoreServiceTest, LoadPreviousSessionAndTabsPinned) {
  CreateSessionServiceWithOneWindow(true);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(&controller(), -1);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2U, service_->entries().size());
  // The first entry should come from the session service.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_TRUE(window->tabs[0].pinned);
  ASSERT_EQ(1U, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].virtual_url());

  // Then the closed tab.
  entry = *(++service_->entries().begin());
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  ASSERT_FALSE(tab->pinned);
  ASSERT_EQ(3U, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].virtual_url());
  EXPECT_TRUE(url2_ == tab->navigations[1].virtual_url());
  EXPECT_TRUE(url3_ == tab->navigations[2].virtual_url());
}

// Creates TabRestoreService::kMaxEntries + 1 windows in the session service
// and makes sure we only get back TabRestoreService::kMaxEntries on restore.
TEST_F(TabRestoreServiceTest, ManyWindowsInSessionService) {
  CreateSessionServiceWithOneWindow(false);

  for (size_t i = 0; i < TabRestoreService::kMaxEntries; ++i)
    AddWindowWithOneTabToSessionService(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(&controller(), -1);

  RecreateService();

  // We should get back kMaxEntries entries. We added more, but
  // TabRestoreService only allows up to kMaxEntries.
  ASSERT_EQ(TabRestoreService::kMaxEntries, service_->entries().size());

  // The first entry should come from the session service.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry);
  ASSERT_EQ(1U, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  EXPECT_EQ(0, window->timestamp.ToInternalValue());
  ASSERT_EQ(1U, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_EQ(0, window->tabs[0].timestamp.ToInternalValue());
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].virtual_url());
}

// Makes sure we restore the time stamp correctly.
TEST_F(TabRestoreServiceTest, TimestampSurvivesRestore) {
  base::Time tab_timestamp(base::Time::FromInternalValue(123456789));

  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(&controller(), -1);

  // Make sure an entry was created.
  ASSERT_EQ(1U, service_->entries().size());

  // Make sure the entry matches.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  Tab* tab = static_cast<Tab*>(entry);
  tab->timestamp = tab_timestamp;

  // Set this, otherwise previous session won't be loaded.
  profile()->set_last_session_exited_cleanly(false);

  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1U, service_->entries().size());

  // And verify the entry.
  TabRestoreService::Entry* restored_entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, restored_entry->type);
  Tab* restored_tab =
      static_cast<Tab*>(restored_entry);
  EXPECT_EQ(tab_timestamp.ToInternalValue(),
            restored_tab->timestamp.ToInternalValue());
}

TEST_F(TabRestoreServiceTest, PruneEntries) {
  service_->ClearEntries();
  ASSERT_TRUE(service_->entries().empty());

  const size_t max_entries = TabRestoreService::kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    TabNavigation navigation;
    navigation.set_virtual_url(GURL(StringPrintf("http://%d",
                                                 static_cast<int>(i))));
    navigation.set_title(ASCIIToUTF16(StringPrintf("%d", static_cast<int>(i))));

    Tab* tab = new Tab();
    tab->navigations.push_back(navigation);
    tab->current_navigation_index = 0;

    service_->entries_.push_back(tab);
  }

  // Only keep kMaxEntries around.
  EXPECT_EQ(max_entries + 5, service_->entries_.size());
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());
  // Pruning again does nothing.
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());

  // Prune older first.
  TabNavigation navigation;
  navigation.set_virtual_url(GURL("http://recent"));
  navigation.set_title(ASCIIToUTF16("Most recent"));
  Tab* tab = new Tab();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  service_->entries_.push_front(tab);
  EXPECT_EQ(max_entries + 1, service_->entries_.size());
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());
  EXPECT_EQ(GURL("http://recent"),
      static_cast<Tab*>(service_->entries_.front())->
          navigations[0].virtual_url());

  // Ignore NTPs.
  navigation.set_virtual_url(GURL(chrome::kChromeUINewTabURL));
  navigation.set_title(ASCIIToUTF16("New tab"));

  tab = new Tab();
  tab->navigations.push_back(navigation);
  tab->current_navigation_index = 0;
  service_->entries_.push_front(tab);

  EXPECT_EQ(max_entries + 1, service_->entries_.size());
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());
  EXPECT_EQ(GURL("http://recent"),
      static_cast<Tab*>(service_->entries_.front())->
          navigations[0].virtual_url());

  // Don't prune pinned NTPs.
  tab = new Tab();
  tab->pinned = true;
  tab->current_navigation_index = 0;
  tab->navigations.push_back(navigation);
  service_->entries_.push_front(tab);
  EXPECT_EQ(max_entries + 1, service_->entries_.size());
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
      static_cast<Tab*>(service_->entries_.front())->
          navigations[0].virtual_url());

  // Don't prune NTPs that have multiple navigations.
  // (Erase the last NTP first.)
  service_->entries_.erase(service_->entries_.begin());
  tab = new Tab();
  tab->current_navigation_index = 1;
  tab->navigations.push_back(navigation);
  tab->navigations.push_back(navigation);
  service_->entries_.push_front(tab);
  EXPECT_EQ(max_entries, service_->entries_.size());
  service_->PruneEntries();
  EXPECT_EQ(max_entries, service_->entries_.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
      static_cast<Tab*>(service_->entries_.front())->
          navigations[1].virtual_url());
}

// Regression test for crbug.com/106082
TEST_F(TabRestoreServiceTest, PruneIsCalled) {
  CreateSessionServiceWithOneWindow(false);

  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  profile()->set_restored_last_session(true);

  const size_t max_entries = TabRestoreService::kMaxEntries;
  for (size_t i = 0; i < max_entries + 5; i++) {
    NavigateAndCommit(GURL(StringPrintf("http://%d", static_cast<int>(i))));
    service_->CreateHistoricalTab(&controller(), -1);
  }

  EXPECT_EQ(max_entries, service_->entries_.size());
  // This should not crash.
  service_->LoadTabsFromLastSession();
  EXPECT_EQ(max_entries, service_->entries_.size());
}
