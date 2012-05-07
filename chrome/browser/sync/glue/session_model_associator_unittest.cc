// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/test_browser_thread.h"
#include "sync/protocol/session_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "sync/util/time.h"

using content::BrowserThread;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace browser_sync {

class SyncSessionModelAssociatorTest : public testing::Test {
 public:
  SyncSessionModelAssociatorTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        sync_service_(&profile_),
        model_associator_(&sync_service_, true) {}

  // Helper methods to avoid having to friend individual tests.
  bool GetFavicon(std::string page_url, std::string* favicon) {
    return model_associator_.GetSyncedFaviconForPageURL(page_url, favicon);
  }

  void LoadTabFavicon(const sync_pb::SessionTab& tab) {
    model_associator_.LoadForeignTabFavicon(tab);
  }

  size_t NumFavicons() {
    return model_associator_.NumFaviconsForTesting();
  }

  void DecrementFavicon(std::string url) {
    model_associator_.DecrementAndCleanFaviconForURL(url);
  }

  void AssociateTabContents(const SyncedWindowDelegate& window,
                            const SyncedTabDelegate& new_tab,
                            SyncedSessionTab* prev_tab,
                            sync_pb::SessionTab* sync_tab,
                            GURL* new_url) {
    model_associator_.AssociateTabContents(window,
                                           new_tab,
                                           prev_tab,
                                           sync_tab,
                                           new_url);
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  NiceMock<ProfileMock> profile_;
  NiceMock<ProfileSyncServiceMock> sync_service_;
  SessionModelAssociator model_associator_;
};

TEST_F(SyncSessionModelAssociatorTest, SessionWindowHasNoTabsToSync) {
  SessionWindow win;
  ASSERT_TRUE(SessionWindowHasNoTabsToSync(win));
  scoped_ptr<SessionTab> tab(new SessionTab());
  win.tabs.push_back(tab.release());
  ASSERT_TRUE(SessionWindowHasNoTabsToSync(win));
  TabNavigation nav(0, GURL("about:bubba"),
                    content::Referrer(GURL("about:referrer"),
                                      WebKit::WebReferrerPolicyDefault),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), content::PageTransitionFromInt(0));
  win.tabs[0]->navigations.push_back(nav);
  ASSERT_FALSE(SessionWindowHasNoTabsToSync(win));
}

TEST_F(SyncSessionModelAssociatorTest, ShouldSyncSessionTab) {
  SyncedSessionTab tab;
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav(0, GURL(chrome::kChromeUINewTabURL),
                    content::Referrer(GURL("about:referrer"),
                                      WebKit::WebReferrerPolicyDefault),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), content::PageTransitionFromInt(0));
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav2(0, GURL("about:bubba"),
                     content::Referrer(GURL("about:referrer"),
                                       WebKit::WebReferrerPolicyDefault),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), content::PageTransitionFromInt(0));
  tab.navigations.push_back(nav2);
  // Once there's another navigation, the tab is valid.
  ASSERT_TRUE(ShouldSyncSessionTab(tab));
}

TEST_F(SyncSessionModelAssociatorTest,
       ShouldSyncSessionTabIgnoresFragmentForNtp) {
  SyncedSessionTab tab;
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav(0, GURL(std::string(chrome::kChromeUINewTabURL) +
                            "#bookmarks"),
                    content::Referrer(GURL("about:referrer"),
                                      WebKit::WebReferrerPolicyDefault),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), content::PageTransitionFromInt(0));
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
}

TEST_F(SyncSessionModelAssociatorTest, PopulateSessionHeader) {
  sync_pb::SessionHeader header_s;
  header_s.set_client_name("Client 1");
  header_s.set_device_type(sync_pb::SessionHeader_DeviceType_TYPE_WIN);

  SyncedSession session;
  base::Time time = base::Time::Now();
  SessionModelAssociator::PopulateSessionHeaderFromSpecifics(
      header_s, time, &session);
  ASSERT_EQ("Client 1", session.session_name);
  ASSERT_EQ(SyncedSession::TYPE_WIN, session.device_type);
  ASSERT_EQ(time, session.modified_time);
}

TEST_F(SyncSessionModelAssociatorTest, PopulateSessionWindow) {
  sync_pb::SessionWindow window_s;
  window_s.add_tab(0);
  window_s.set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  window_s.set_selected_tab_index(1);

  std::string tag = "tag";
  SyncedSessionTracker tracker;
  SyncedSession* session = tracker.GetSession(tag);
  tracker.PutWindowInSession(tag, 0);
  SessionModelAssociator::PopulateSessionWindowFromSpecifics(
      tag, window_s, base::Time(), session->windows[0], &tracker);
  ASSERT_EQ(1U, session->windows[0]->tabs.size());
  ASSERT_EQ(1, session->windows[0]->selected_tab_index);
  ASSERT_EQ(1, session->windows[0]->type);
  ASSERT_EQ(1U, tracker.num_synced_sessions());
  ASSERT_EQ(1U, tracker.num_synced_tabs(std::string("tag")));
}

TEST_F(SyncSessionModelAssociatorTest, PopulateSessionTab) {
  sync_pb::SessionTab tab_s;
  tab_s.set_tab_id(5);
  tab_s.set_tab_visual_index(13);
  tab_s.set_current_navigation_index(3);
  tab_s.set_pinned(true);
  tab_s.set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab_s.add_navigation();
  navigation->set_virtual_url("http://foo/1");
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::TabNavigation_PageTransition_TYPED);

  SyncedSessionTab tab;
  tab.tab_id.set_id(5);  // Expected to be set by the SyncedSessionTracker.
  SessionModelAssociator::PopulateSessionTabFromSpecifics(
      tab_s, base::Time(), &tab);
  ASSERT_EQ(5, tab.tab_id.id());
  ASSERT_EQ(13, tab.tab_visual_index);
  ASSERT_EQ(3, tab.current_navigation_index);
  ASSERT_TRUE(tab.pinned);
  ASSERT_EQ("app_id", tab.extension_app_id);
  ASSERT_EQ(GURL("referrer"), tab.navigations[0].referrer().url);
  ASSERT_EQ(string16(ASCIIToUTF16("title")), tab.navigations[0].title());
  ASSERT_EQ(content::PAGE_TRANSITION_TYPED, tab.navigations[0].transition());
  ASSERT_EQ(GURL("http://foo/1"), tab.navigations[0].virtual_url());
}

TEST_F(SyncSessionModelAssociatorTest, TabNodePool) {
  SessionModelAssociator::TabNodePool pool(NULL);
  pool.set_machine_tag("tag");
  ASSERT_TRUE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(0U, pool.capacity());
  pool.AddTabNode(5);
  pool.AddTabNode(10);
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(10, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(5, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_TRUE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  // Release them in reverse order.
  pool.FreeTabNode(10);
  pool.FreeTabNode(5);
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(5, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(10, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_TRUE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  // Release them again.
  pool.FreeTabNode(10);
  pool.FreeTabNode(5);
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  pool.clear();
  ASSERT_TRUE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(0U, pool.capacity());
}

namespace {

class SyncedWindowDelegateMock : public SyncedWindowDelegate {
 public:
  SyncedWindowDelegateMock() {}
  virtual ~SyncedWindowDelegateMock() {}
  MOCK_CONST_METHOD0(HasWindow, bool());
  MOCK_CONST_METHOD0(GetSessionId, SessionID::id_type());
  MOCK_CONST_METHOD0(GetTabCount, int());
  MOCK_CONST_METHOD0(GetActiveIndex, int());
  MOCK_CONST_METHOD0(IsApp, bool());
  MOCK_CONST_METHOD0(IsTypeTabbed, bool());
  MOCK_CONST_METHOD0(IsTypePopup, bool());
  MOCK_CONST_METHOD1(IsTabPinned, bool(const SyncedTabDelegate* tab));
  MOCK_CONST_METHOD1(GetTabAt, SyncedTabDelegate*(int index));
  MOCK_CONST_METHOD1(GetTabIdAt, SessionID::id_type(int index));
};

class SyncedTabDelegateMock : public SyncedTabDelegate {
 public:
  SyncedTabDelegateMock() {}
  virtual ~SyncedTabDelegateMock() {}

  MOCK_CONST_METHOD0(GetWindowId, SessionID::id_type());
  MOCK_CONST_METHOD0(GetSessionId, SessionID::id_type());
  MOCK_CONST_METHOD0(IsBeingDestroyed, bool());
  MOCK_CONST_METHOD0(profile, Profile*());
  MOCK_CONST_METHOD0(HasExtensionAppId, bool());
  MOCK_CONST_METHOD0(GetExtensionAppId, const std::string&());
  MOCK_CONST_METHOD0(GetCurrentEntryIndex, int());
  MOCK_CONST_METHOD0(GetEntryCount, int());
  MOCK_CONST_METHOD0(GetPendingEntryIndex, int());
  MOCK_CONST_METHOD0(GetPendingEntry, content::NavigationEntry*());
  MOCK_CONST_METHOD1(GetEntryAtIndex, content::NavigationEntry*(int i));
  MOCK_CONST_METHOD0(GetActiveEntry, content::NavigationEntry*());
};

class SyncRefreshListener : public content::NotificationObserver {
 public:
  SyncRefreshListener() : notified_of_refresh_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
        content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_SYNC_REFRESH_LOCAL) {
      notified_of_refresh_ = true;
    }
  }

  bool notified_of_refresh() const { return notified_of_refresh_; }

 private:
  bool notified_of_refresh_;
  content::NotificationRegistrar registrar_;
};

}  // namespace.

// Test that AttemptSessionsDataRefresh() triggers the
// NOTIFICATION_SYNC_REFRESH_LOCAL notification.
TEST_F(SyncSessionModelAssociatorTest, TriggerSessionRefresh) {
  SyncRefreshListener refresh_listener;

  EXPECT_FALSE(refresh_listener.notified_of_refresh());
  model_associator_.AttemptSessionsDataRefresh();
  EXPECT_TRUE(refresh_listener.notified_of_refresh());
}

// Test that we exclude tabs with only chrome:// and file:// schemed navigations
// from ShouldSyncTab(..).
TEST_F(SyncSessionModelAssociatorTest, ValidTabs) {
  NiceMock<SyncedTabDelegateMock> tab_mock;

  // A null entry shouldn't crash.
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return((content::NavigationEntry *)NULL));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(1));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_FALSE(model_associator_.ShouldSyncTab(tab_mock));

  // A chrome:// entry isn't valid.
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("chrome://preferences/"));
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(Return(entry.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(1));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_FALSE(model_associator_.ShouldSyncTab(tab_mock));

  // A file:// entry isn't valid, even in addition to another entry.
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("file://bla"));
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(Return(entry.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_FALSE(model_associator_.ShouldSyncTab(tab_mock));

  // Add a valid scheme entry to tab, making the tab valid.
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.google.com"));
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_TRUE(model_associator_.ShouldSyncTab(tab_mock));
}

// Create tab specifics with an empty favicon. Ensure it gets ignored and not
// stored into the synced favicon lookups.
TEST_F(SyncSessionModelAssociatorTest, LoadEmptyFavicon) {
  std::string favicon = "";
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  std::string synced_favicon;
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  LoadTabFavicon(tab);
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
}

// Create tab specifics with a non-web favicon. Ensure it gets ignored and not
// stored into the synced favicon lookups.
TEST_F(SyncSessionModelAssociatorTest, LoadNonWebFavicon) {
  std::string favicon = "these are icon synced_favicon";
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  // Set favicon type to an unsupported value (1 == WEB_FAVICON).
   tab.mutable_unknown_fields()->AddVarint(9, 2);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  std::string synced_favicon;
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  LoadTabFavicon(tab);
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
}

// Create tab specifics with a valid favicon. Ensure it gets stored in the
// synced favicon lookups and is accessible by the page url.
TEST_F(SyncSessionModelAssociatorTest, LoadValidFavicon) {
  std::string favicon = "these are icon synced_favicon";
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  std::string synced_favicon;
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  LoadTabFavicon(tab);
  EXPECT_TRUE(GetFavicon(page_url, &synced_favicon));
  ASSERT_FALSE(synced_favicon.empty());
  EXPECT_EQ(favicon, synced_favicon);
}

// Create tab specifics with a valid favicon, load it, then load tab specifics
// with a new favicon for the same favicon source but different page. Ensure the
// new favicon overwrites the old favicon for both page urls.
TEST_F(SyncSessionModelAssociatorTest, UpdateValidFavicon) {
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";

  std::string favicon = "these are icon synced_favicon";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  std::string synced_favicon;
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  LoadTabFavicon(tab);
  EXPECT_TRUE(GetFavicon(page_url, &synced_favicon));
  ASSERT_FALSE(synced_favicon.empty());
  EXPECT_EQ(favicon, synced_favicon);

  // Now have a new page with same favicon source but newer favicon data.
  std::string favicon2 = "these are new icon synced_favicon";
  std::string page_url2 = "http://www.faviconurl.com/page2.html";
  sync_pb::SessionTab tab2;
  tab2.set_favicon(favicon2);
  tab2.set_favicon_source(favicon_url);
  tab2.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation2 = tab2.add_navigation();
  navigation2->set_virtual_url(page_url2);
  tab2.set_current_navigation_index(0);

  // Verify the favicons for both pages match the newest favicon.
  synced_favicon.clear();
  EXPECT_FALSE(GetFavicon(page_url2, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  LoadTabFavicon(tab2);
  EXPECT_TRUE(GetFavicon(page_url2, &synced_favicon));
  ASSERT_FALSE(synced_favicon.empty());
  EXPECT_EQ(favicon2, synced_favicon);
  EXPECT_NE(favicon, synced_favicon);
  synced_favicon.clear();
  EXPECT_TRUE(GetFavicon(page_url, &synced_favicon));
  ASSERT_FALSE(synced_favicon.empty());
  EXPECT_EQ(favicon2, synced_favicon);
  EXPECT_NE(favicon, synced_favicon);
}

// Ensure that favicon cleanup cleans up favicons no longer being used and
// doesn't touch those favicons still in use.
TEST_F(SyncSessionModelAssociatorTest, FaviconCleanup) {
  EXPECT_EQ(NumFavicons(), 0U);

  std::string double_favicon = "these are icon synced_favicon";
  std::string double_favicon_url = "http://www.faviconurl.com/favicon.ico";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(double_favicon);
  tab.set_favicon_source(double_favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);
  LoadTabFavicon(tab);
  EXPECT_EQ(1U, NumFavicons());

  // Add another page using the first favicon.
  std::string page_url2 = "http://www.faviconurl.com/page2.html";
  tab.mutable_navigation(0)->set_virtual_url(page_url2);
  LoadTabFavicon(tab);
  EXPECT_EQ(1U, NumFavicons());

  // Add a favicon with a single user.
  std::string single_favicon = "different favicon synced_favicon";
  std::string single_favicon_url = "http://www.single_favicon_page.com/x.ico";
  std::string single_favicon_page = "http://www.single_favicon_page.com/x.html";
  tab.set_favicon(single_favicon);
  tab.set_favicon_source(single_favicon_url);
  tab.mutable_navigation(0)->set_virtual_url(single_favicon_page);
  LoadTabFavicon(tab);
  EXPECT_EQ(2U, NumFavicons());

  // Decrementing a favicon used by one page should remove it.
  std::string synced_favicon;
  EXPECT_TRUE(GetFavicon(single_favicon_page, &synced_favicon));
  EXPECT_EQ(synced_favicon, single_favicon);
  DecrementFavicon(single_favicon_page);
  synced_favicon.clear();
  EXPECT_FALSE(GetFavicon(single_favicon_page, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  EXPECT_EQ(1U, NumFavicons());

  // Decrementing a favicon used by two pages shouldn't remove it.
  synced_favicon.clear();
  EXPECT_TRUE(GetFavicon(page_url, &synced_favicon));
  EXPECT_EQ(synced_favicon, double_favicon);
  synced_favicon.clear();
  EXPECT_TRUE(GetFavicon(page_url2, &synced_favicon));
  EXPECT_EQ(synced_favicon, double_favicon);
  DecrementFavicon(page_url);
  EXPECT_EQ(1U, NumFavicons());
  synced_favicon.clear();
  EXPECT_TRUE(GetFavicon(page_url, &synced_favicon));
  EXPECT_EQ(synced_favicon, double_favicon);
  synced_favicon.clear();
  EXPECT_TRUE(GetFavicon(page_url2, &synced_favicon));
  EXPECT_EQ(synced_favicon, double_favicon);
  EXPECT_EQ(1U, NumFavicons());

  // Attempting to decrement a page that's already removed should do nothing.
  DecrementFavicon(single_favicon_page);
  EXPECT_EQ(1U, NumFavicons());

  // Attempting to decrement an empty url should do nothing.
  DecrementFavicon("");
  EXPECT_EQ(1U, NumFavicons());

  // Decrementing the second and only remaining page should remove the favicon.
  // Both pages that referred to it should now fail to look up their favicon.
  DecrementFavicon(page_url2);
  synced_favicon.clear();
  EXPECT_FALSE(GetFavicon(page_url, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  EXPECT_EQ(0U, NumFavicons());
  synced_favicon.clear();
  EXPECT_FALSE(GetFavicon(page_url2, &synced_favicon));
  EXPECT_TRUE(synced_favicon.empty());
  EXPECT_EQ(0U, NumFavicons());
}

// Ensure new tabs have the current timestamp set for the current navigation,
// while other navigations have timestamp zero.
TEST_F(SyncSessionModelAssociatorTest, AssociateNewTab) {
  NiceMock<SyncedWindowDelegateMock> window_mock;
  EXPECT_CALL(window_mock, IsTabPinned(_)).WillRepeatedly(Return(false));

  // Create a tab with three valid entries.
  NiceMock<SyncedTabDelegateMock> tab_mock;
  EXPECT_CALL(tab_mock, GetSessionId()).WillRepeatedly(Return(0));
  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry1.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  // This tab is new, so prev_tab is the default SyncedSessionTab object.
  SyncedSessionTab prev_tab;
  prev_tab.tab_id.set_id(0);

  sync_pb::SessionTab sync_tab;
  GURL new_url;
  int64 now = TimeToProtoTime(base::Time::Now());
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  EXPECT_EQ(new_url, entry3->GetVirtualURL());
  ASSERT_EQ(3, sync_tab.navigation_size());
  EXPECT_EQ(entry1->GetVirtualURL().spec(),
            sync_tab.navigation(0).virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL().spec(),
            sync_tab.navigation(1).virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL().spec(),
            sync_tab.navigation(2).virtual_url());
  EXPECT_EQ(2, sync_tab.current_navigation_index());
  EXPECT_LE(0, sync_tab.navigation(0).timestamp());
  EXPECT_LE(0, sync_tab.navigation(1).timestamp());
  EXPECT_LE(now, sync_tab.navigation(2).timestamp());
}

// Ensure we preserve old timestamps when the entries don't change.
TEST_F(SyncSessionModelAssociatorTest, AssociateExistingTab) {
  NiceMock<SyncedWindowDelegateMock> window_mock;
  EXPECT_CALL(window_mock, IsTabPinned(_)).WillRepeatedly(Return(false));

  // Create a tab with three valid entries.
  NiceMock<SyncedTabDelegateMock> tab_mock;
  EXPECT_CALL(tab_mock, GetSessionId()).WillRepeatedly(Return(0));
  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry1.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  // This tab is new, so prev_tab is the default SyncedSessionTab object.
  SyncedSessionTab prev_tab;
  prev_tab.tab_id.set_id(0);

  // The initial AssociateTabContents call builds the prev_tab.
  sync_pb::SessionTab sync_tab;
  GURL new_url;
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  // Override the timestamps to arbitrary old values we can compare against.
  prev_tab.synced_tab_navigations[0].set_timestamp(ProtoTimeToTime(1));
  prev_tab.synced_tab_navigations[1].set_timestamp(ProtoTimeToTime(2));
  prev_tab.synced_tab_navigations[2].set_timestamp(ProtoTimeToTime(3));

  // Now re-associate with the same data.
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  EXPECT_EQ(new_url, entry3->GetVirtualURL());
  ASSERT_EQ(3, sync_tab.navigation_size());
  EXPECT_EQ(entry1->GetVirtualURL().spec(),
            sync_tab.navigation(0).virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL().spec(),
            sync_tab.navigation(1).virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL().spec(),
            sync_tab.navigation(2).virtual_url());
  EXPECT_EQ(2, sync_tab.current_navigation_index());
  EXPECT_EQ(1, sync_tab.navigation(0).timestamp());
  EXPECT_EQ(2, sync_tab.navigation(1).timestamp());
  EXPECT_EQ(3, sync_tab.navigation(2).timestamp());
  EXPECT_EQ(3U, prev_tab.navigations.size());
  EXPECT_EQ(3U, prev_tab.synced_tab_navigations.size());
}

// Ensure we add a fresh timestamp for new entries appended to the end.
TEST_F(SyncSessionModelAssociatorTest, AssociateAppendedTab) {
    NiceMock<SyncedWindowDelegateMock> window_mock;
  EXPECT_CALL(window_mock, IsTabPinned(_)).WillRepeatedly(Return(false));

  // Create a tab with three valid entries.
  NiceMock<SyncedTabDelegateMock> tab_mock;
  EXPECT_CALL(tab_mock, GetSessionId()).WillRepeatedly(Return(0));
  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry1.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  // This tab is new, so prev_tab is the default SyncedSessionTab object.
  SyncedSessionTab prev_tab;
  prev_tab.tab_id.set_id(0);

  // The initial AssociateTabContents call builds the prev_tab.
  sync_pb::SessionTab sync_tab;
  GURL new_url;
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  // Override the timestamps to arbitrary old values we can compare against.
  prev_tab.synced_tab_navigations[0].set_timestamp(ProtoTimeToTime(1));
  prev_tab.synced_tab_navigations[1].set_timestamp(ProtoTimeToTime(2));
  prev_tab.synced_tab_navigations[2].set_timestamp(ProtoTimeToTime(3));

  // Add a new entry and change the current navigation index.
  scoped_ptr<content::NavigationEntry> entry4(
      content::NavigationEntry::Create());
  entry4->SetVirtualURL(GURL("http://www.poodle.com"));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(3)).WillRepeatedly(
      Return(entry4.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(4));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(3));

  // The new entry should have a timestamp later than this.
  int64 now = TimeToProtoTime(base::Time::Now());

  // Now re-associate with the new version.
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  EXPECT_EQ(new_url, entry4->GetVirtualURL());
  ASSERT_EQ(4, sync_tab.navigation_size());
  EXPECT_EQ(entry1->GetVirtualURL().spec(),
            sync_tab.navigation(0).virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL().spec(),
            sync_tab.navigation(1).virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL().spec(),
            sync_tab.navigation(2).virtual_url());
  EXPECT_EQ(entry4->GetVirtualURL().spec(),
            sync_tab.navigation(3).virtual_url());
  EXPECT_EQ(3, sync_tab.current_navigation_index());
  EXPECT_EQ(1, sync_tab.navigation(0).timestamp());
  EXPECT_EQ(2, sync_tab.navigation(1).timestamp());
  EXPECT_EQ(3, sync_tab.navigation(2).timestamp());
  EXPECT_LE(now, sync_tab.navigation(3).timestamp());
  EXPECT_EQ(4U, prev_tab.navigations.size());
  EXPECT_EQ(4U, prev_tab.synced_tab_navigations.size());
}

// We shouldn't get confused when old/new entries from the previous tab have
// been pruned in the new tab. Timestamps for old entries we move back to in the
// navigation stack should be refreshed.
TEST_F(SyncSessionModelAssociatorTest, AssociatePrunedTab) {
    NiceMock<SyncedWindowDelegateMock> window_mock;
  EXPECT_CALL(window_mock, IsTabPinned(_)).WillRepeatedly(Return(false));

  // Create a tab with four valid entries.
  NiceMock<SyncedTabDelegateMock> tab_mock;
  EXPECT_CALL(tab_mock, GetSessionId()).WillRepeatedly(Return(0));
  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  scoped_ptr<content::NavigationEntry> entry4(
      content::NavigationEntry::Create());
  entry4->SetVirtualURL(GURL("http://www.poodle.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry1.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(3)).WillRepeatedly(
      Return(entry4.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(4));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  // This tab is new, so prev_tab is the default SyncedSessionTab object.
  SyncedSessionTab prev_tab;
  prev_tab.tab_id.set_id(0);

  // The initial AssociateTabContents call builds the prev_tab.
  sync_pb::SessionTab sync_tab;
  GURL new_url;
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  // Override the timestamps to arbitrary old values we can compare against.
  prev_tab.synced_tab_navigations[0].set_timestamp(ProtoTimeToTime(1));
  prev_tab.synced_tab_navigations[1].set_timestamp(ProtoTimeToTime(2));
  prev_tab.synced_tab_navigations[2].set_timestamp(ProtoTimeToTime(3));
  prev_tab.synced_tab_navigations[2].set_timestamp(ProtoTimeToTime(4));

  // Reset new tab to have the oldest entry pruned, the current navigation
  // set to entry3, and a new entry added in place of entry4.
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(1));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry3.get()));
  scoped_ptr<content::NavigationEntry> entry5(
      content::NavigationEntry::Create());
  entry5->SetVirtualURL(GURL("http://www.noogle.com"));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry5.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  // The new entry should have a timestamp later than this.
  int64 now = TimeToProtoTime(base::Time::Now());

  // Now re-associate with the new version.
  AssociateTabContents(window_mock, tab_mock, &prev_tab, &sync_tab, &new_url);

  // Only entry2's timestamp should be preserved. The new entry should have a
  // new timestamp.
  EXPECT_EQ(new_url, entry3->GetVirtualURL());
  ASSERT_EQ(3, sync_tab.navigation_size());
  EXPECT_EQ(entry2->GetVirtualURL().spec(),
            sync_tab.navigation(0).virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL().spec(),
            sync_tab.navigation(1).virtual_url());
  EXPECT_EQ(entry5->GetVirtualURL().spec(),
            sync_tab.navigation(2).virtual_url());
  EXPECT_EQ(1, sync_tab.current_navigation_index());
  EXPECT_EQ(2, sync_tab.navigation(0).timestamp());
  EXPECT_LE(now, sync_tab.navigation(1).timestamp());
  EXPECT_LE(now, sync_tab.navigation(2).timestamp());
  EXPECT_EQ(3U, prev_tab.navigations.size());
  EXPECT_EQ(3U, prev_tab.synced_tab_navigations.size());
}

}  // namespace browser_sync
