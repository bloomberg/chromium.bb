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
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::NiceMock;
using testing::Return;

namespace browser_sync {

typedef testing::Test SessionModelAssociatorTest;

TEST_F(SessionModelAssociatorTest, SessionWindowHasNoTabsToSync) {
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

TEST_F(SessionModelAssociatorTest, ShouldSyncSessionTab) {
  SessionTab tab;
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

TEST_F(SessionModelAssociatorTest, ShouldSyncSessionTabIgnoresFragmentForNtp) {
  SessionTab tab;
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

TEST_F(SessionModelAssociatorTest, PopulateSessionHeader) {
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

TEST_F(SessionModelAssociatorTest, PopulateSessionWindow) {
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

TEST_F(SessionModelAssociatorTest, PopulateSessionTab) {
  sync_pb::SessionTab tab_s;
  tab_s.set_tab_id(5);
  tab_s.set_tab_visual_index(13);
  tab_s.set_current_navigation_index(3);
  tab_s.set_pinned(true);
  tab_s.set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab_s.add_navigation();
  navigation->set_index(12);
  navigation->set_virtual_url("http://foo/1");
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::TabNavigation_PageTransition_TYPED);

  SessionTab tab;
  tab.tab_id.set_id(5);  // Expected to be set by the SyncedSessionTracker.
  SessionModelAssociator::PopulateSessionTabFromSpecifics(
      tab_s, base::Time(), &tab);
  ASSERT_EQ(5, tab.tab_id.id());
  ASSERT_EQ(13, tab.tab_visual_index);
  ASSERT_EQ(3, tab.current_navigation_index);
  ASSERT_TRUE(tab.pinned);
  ASSERT_EQ("app_id", tab.extension_app_id);
  ASSERT_EQ(12, tab.navigations[0].index());
  ASSERT_EQ(GURL("referrer"), tab.navigations[0].referrer().url);
  ASSERT_EQ(string16(ASCIIToUTF16("title")), tab.navigations[0].title());
  ASSERT_EQ(content::PAGE_TRANSITION_TYPED, tab.navigations[0].transition());
  ASSERT_EQ(GURL("http://foo/1"), tab.navigations[0].virtual_url());
}

TEST_F(SessionModelAssociatorTest, TabNodePool) {
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

class SyncedTabDelegateMock : public SyncedTabDelegate {
 public:
  SyncedTabDelegateMock() {}
  ~SyncedTabDelegateMock() {}

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

}  // namespace.

TEST_F(SessionModelAssociatorTest, ValidTabs) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  SessionModelAssociator model_associator(NULL, true);

  NiceMock<SyncedTabDelegateMock> tab_mock;

  // A null entry shouldn't crash.
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(
      Return((content::NavigationEntry *)NULL));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(1));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_FALSE(model_associator.ShouldSyncTab(tab_mock));

  // A chrome:// entry isn't valid.
  content::NavigationEntry* entry = content::NavigationEntry::Create();
  entry->SetVirtualURL(GURL("chrome://preferences/"));
  EXPECT_FALSE(model_associator.ShouldSyncTab(tab_mock));

  // A file:// entry isn't valid, even in addition to another entry.
  content::NavigationEntry* entry2 = content::NavigationEntry::Create();
  entry2->SetVirtualURL(GURL("file://bla"));
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(Return(entry));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(Return(entry2));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(2));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_FALSE(model_associator.ShouldSyncTab(tab_mock));

  // Add a valid scheme entry to tab, making the tab valid.
  content::NavigationEntry* entry3 = content::NavigationEntry::Create();
  entry3->SetVirtualURL(GURL("http://www.google.com"));
  testing::Mock::VerifyAndClearExpectations(&tab_mock);
  EXPECT_CALL(tab_mock, GetCurrentEntryIndex()).WillRepeatedly(Return(0));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(0)).WillRepeatedly(Return(entry));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(1)).WillRepeatedly(Return(entry2));
  EXPECT_CALL(tab_mock, GetEntryAtIndex(2)).WillRepeatedly(Return(entry3));
  EXPECT_CALL(tab_mock, GetEntryCount()).WillRepeatedly(Return(3));
  EXPECT_CALL(tab_mock, GetPendingEntryIndex()).WillRepeatedly(Return(-1));
  EXPECT_TRUE(model_associator.ShouldSyncTab(tab_mock));
}

}  // namespace browser_sync
