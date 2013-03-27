// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
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
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/util/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace browser_sync {

class SyncSessionModelAssociatorTest : public testing::Test {
 protected:
  SyncSessionModelAssociatorTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        sync_service_(&profile_),
        model_associator_(&sync_service_, true) {}

  void LoadTabFavicon(const sync_pb::SessionTab& tab) {
    model_associator_.LoadForeignTabFavicon(tab);
    message_loop_.RunUntilIdle();
  }

  static GURL GetCurrentVirtualURL(const SyncedTabDelegate& tab_delegate) {
    return SessionModelAssociator::GetCurrentVirtualURL(tab_delegate);
  }

  static void SetSessionTabFromDelegate(
      const SyncedTabDelegate& tab_delegate,
      base::Time mtime,
      SessionTab* session_tab) {
    SessionModelAssociator::SetSessionTabFromDelegate(
        tab_delegate,
        mtime,
        session_tab);
  }

  bool FaviconEquals(const GURL page_url,
                     std::string expected_bytes) {
    FaviconCache* cache = model_associator_.GetFaviconCache();
    GURL gurl(page_url);
    scoped_refptr<base::RefCountedMemory> favicon;
    if (!cache->GetSyncedFaviconForPageURL(gurl, &favicon))
      return expected_bytes.empty();
    if (favicon->size() != expected_bytes.size())
      return false;
    for (size_t i = 0; i < favicon->size(); ++i) {
      if (expected_bytes[i] != *(favicon->front() + i))
        return false;
    }
    return true;
  }

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  NiceMock<ProfileMock> profile_;
  NiceMock<ProfileSyncServiceMock> sync_service_;

 protected:
  SessionModelAssociator model_associator_;
};

namespace {

TEST_F(SyncSessionModelAssociatorTest, SessionWindowHasNoTabsToSync) {
  SessionWindow win;
  ASSERT_TRUE(SessionWindowHasNoTabsToSync(win));
  scoped_ptr<SessionTab> tab(new SessionTab());
  win.tabs.push_back(tab.release());
  ASSERT_TRUE(SessionWindowHasNoTabsToSync(win));
  TabNavigation nav =
      SessionTypesTestHelper::CreateNavigation("about:bubba", "title");
  win.tabs[0]->navigations.push_back(nav);
  ASSERT_FALSE(SessionWindowHasNoTabsToSync(win));
}

TEST_F(SyncSessionModelAssociatorTest, ShouldSyncSessionTab) {
  SessionTab tab;
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav =
      SessionTypesTestHelper::CreateNavigation(
          chrome::kChromeUINewTabURL, "title");
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav2 =
      SessionTypesTestHelper::CreateNavigation("about:bubba", "title");
  tab.navigations.push_back(nav2);
  // Once there's another navigation, the tab is valid.
  ASSERT_TRUE(ShouldSyncSessionTab(tab));
}

TEST_F(SyncSessionModelAssociatorTest,
       ShouldSyncSessionTabIgnoresFragmentForNtp) {
  SessionTab tab;
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
  TabNavigation nav =
      SessionTypesTestHelper::CreateNavigation(
          std::string(chrome::kChromeUINewTabURL) + "#bookmarks", "title");
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(ShouldSyncSessionTab(tab));
}

}  // namespace

TEST_F(SyncSessionModelAssociatorTest, PopulateSessionHeader) {
  sync_pb::SessionHeader header_s;
  header_s.set_client_name("Client 1");
  header_s.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);

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

namespace {

class SyncedTabDelegateMock : public SyncedTabDelegate {
 public:
  SyncedTabDelegateMock() {}
  virtual ~SyncedTabDelegateMock() {}

  MOCK_CONST_METHOD0(GetWindowId, SessionID::id_type());
  MOCK_CONST_METHOD0(GetSessionId, SessionID::id_type());
  MOCK_CONST_METHOD0(IsBeingDestroyed, bool());
  MOCK_CONST_METHOD0(profile, Profile*());
  MOCK_CONST_METHOD0(GetExtensionAppId, std::string());
  MOCK_CONST_METHOD0(GetCurrentEntryIndex, int());
  MOCK_CONST_METHOD0(GetEntryCount, int());
  MOCK_CONST_METHOD0(GetPendingEntryIndex, int());
  MOCK_CONST_METHOD0(GetPendingEntry, content::NavigationEntry*());
  MOCK_CONST_METHOD1(GetEntryAtIndex, content::NavigationEntry*(int i));
  MOCK_CONST_METHOD0(GetActiveEntry, content::NavigationEntry*());
  MOCK_CONST_METHOD0(IsPinned, bool());
};

class SyncRefreshListener : public content::NotificationObserver {
 public:
  SyncRefreshListener() : notified_of_refresh_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
        content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_SYNC_REFRESH_LOCAL) {
      notified_of_refresh_ = true;
    }
  }

  bool notified_of_refresh() const { return notified_of_refresh_; }

 private:
  bool notified_of_refresh_;
  content::NotificationRegistrar registrar_;
};

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

// TODO(akalin): We should really use a fake for SyncedTabDelegate.

// Make sure GetCurrentVirtualURL() returns the virtual URL of the pending
// entry if the current entry is pending.
TEST_F(SyncSessionModelAssociatorTest, GetCurrentVirtualURLPending) {
  StrictMock<SyncedTabDelegateMock> tab_mock;
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("http://www.google.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillOnce(Return(0));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillOnce(Return(0));
  EXPECT_CALL(tab_mock, GetPendingEntry()).WillOnce(Return(entry.get()));
  EXPECT_EQ(entry->GetVirtualURL(), GetCurrentVirtualURL(tab_mock));
}

// Make sure GetCurrentVirtualURL() returns the virtual URL of the current
// entry if the current entry is non-pending.
TEST_F(SyncSessionModelAssociatorTest, GetCurrentVirtualURLNonPending) {
  StrictMock<SyncedTabDelegateMock> tab_mock;
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("http://www.google.com"));
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillOnce(Return(0));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillOnce(Return(-1));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillOnce(Return(entry.get()));
  EXPECT_EQ(entry->GetVirtualURL(), GetCurrentVirtualURL(tab_mock));
}

const base::Time kTime1 = base::Time::FromInternalValue(100);
const base::Time kTime2 = base::Time::FromInternalValue(105);
const base::Time kTime3 = base::Time::FromInternalValue(110);
const base::Time kTime4 = base::Time::FromInternalValue(120);
const base::Time kTime5 = base::Time::FromInternalValue(130);

// Populate the mock tab delegate with some data and navigation
// entries and make sure that setting a SessionTab from it preserves
// those entries (and clobbers any existing data).
TEST_F(SyncSessionModelAssociatorTest, SetSessionTabFromDelegate) {
  // Create a tab with three valid entries.
  NiceMock<SyncedTabDelegateMock> tab_mock;
  EXPECT_CALL(tab_mock, GetSessionId()).WillRepeatedly(Return(0));
  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  entry1->SetTimestamp(kTime1);
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  entry2->SetTimestamp(kTime2);
  scoped_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  entry3->SetTimestamp(kTime3);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return(entry1.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(
      Return(entry2.get()));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(
      Return(entry3.get()));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));

  SessionTab session_tab;
  session_tab.window_id.set_id(1);
  session_tab.tab_id.set_id(1);
  session_tab.tab_visual_index = 1;
  session_tab.current_navigation_index = 1;
  session_tab.pinned = true;
  session_tab.extension_app_id = "app id";
  session_tab.user_agent_override = "override";
  session_tab.timestamp = kTime5;
  session_tab.navigations.push_back(
      SessionTypesTestHelper::CreateNavigation(
          "http://www.example.com", "Example"));
  session_tab.session_storage_persistent_id = "persistent id";
  SetSessionTabFromDelegate(tab_mock, kTime4, &session_tab);

  EXPECT_EQ(0, session_tab.window_id.id());
  EXPECT_EQ(0, session_tab.tab_id.id());
  EXPECT_EQ(0, session_tab.tab_visual_index);
  EXPECT_EQ(2, session_tab.current_navigation_index);
  EXPECT_FALSE(session_tab.pinned);
  EXPECT_TRUE(session_tab.extension_app_id.empty());
  EXPECT_TRUE(session_tab.user_agent_override.empty());
  EXPECT_EQ(kTime4, session_tab.timestamp);
  ASSERT_EQ(3u, session_tab.navigations.size());
  EXPECT_EQ(entry1->GetVirtualURL(),
            session_tab.navigations[0].virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL(),
            session_tab.navigations[1].virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL(),
            session_tab.navigations[2].virtual_url());
  EXPECT_EQ(kTime1,
            SessionTypesTestHelper::GetTimestamp(session_tab.navigations[0]));
  EXPECT_EQ(kTime2,
            SessionTypesTestHelper::GetTimestamp(session_tab.navigations[1]));
  EXPECT_EQ(kTime3,
            SessionTypesTestHelper::GetTimestamp(session_tab.navigations[2]));
  EXPECT_TRUE(session_tab.session_storage_persistent_id.empty());
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

  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
  LoadTabFavicon(tab);
  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
}

// Create tab specifics with a non-web favicon. Ensure it gets ignored and not
// stored into the synced favicon lookups.
TEST_F(SyncSessionModelAssociatorTest, LoadNonWebFavicon) {
  std::string favicon = "icon bytes";
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

  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
  LoadTabFavicon(tab);
  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
}

// Create tab specifics with a valid favicon. Ensure it gets stored in the
// synced favicon lookups and is accessible by the page url.
TEST_F(SyncSessionModelAssociatorTest, LoadValidFavicon) {
  std::string favicon = "icon bytes";
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
  LoadTabFavicon(tab);
  EXPECT_TRUE(FaviconEquals(GURL(page_url), favicon));
}

// Create tab specifics with a valid favicon, load it, then load tab specifics
// with a new favicon for the same favicon source but different page. Ensure the
// old favicon remains.
TEST_F(SyncSessionModelAssociatorTest, UpdateValidFavicon) {
  std::string favicon_url = "http://www.faviconurl.com/favicon.ico";

  std::string favicon = "icon bytes";
  std::string page_url = "http://www.faviconurl.com/page.html";
  sync_pb::SessionTab tab;
  tab.set_favicon(favicon);
  tab.set_favicon_source(favicon_url);
  tab.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url);
  tab.set_current_navigation_index(0);

  EXPECT_TRUE(FaviconEquals(GURL(page_url), std::string()));
  LoadTabFavicon(tab);
  EXPECT_TRUE(FaviconEquals(GURL(page_url), favicon));

  // Now have a new page with same favicon source but newer favicon data.
  std::string favicon2 = "icon bytes 2";
  std::string page_url2 = "http://www.faviconurl.com/page2.html";
  sync_pb::SessionTab tab2;
  tab2.set_favicon(favicon2);
  tab2.set_favicon_source(favicon_url);
  tab2.set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  sync_pb::TabNavigation* navigation2 = tab2.add_navigation();
  navigation2->set_virtual_url(page_url2);
  tab2.set_current_navigation_index(0);

  // The new page should be mapped to the old favicon data.
  EXPECT_TRUE(FaviconEquals(GURL(page_url2), std::string()));
  LoadTabFavicon(tab2);
  EXPECT_TRUE(FaviconEquals(GURL(page_url), favicon));
  EXPECT_TRUE(FaviconEquals(GURL(page_url2), favicon));
}

}  // namespace

}  // namespace browser_sync
