// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/common/url_constants.h"
#include "content/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::SessionModelAssociator;
using browser_sync::SyncedSessionTracker;

namespace browser_sync {

typedef testing::Test SessionModelAssociatorTest;

TEST_F(SessionModelAssociatorTest, SessionWindowHasNoTabsToSync) {
  SessionWindow win;
  ASSERT_TRUE(SessionModelAssociator::SessionWindowHasNoTabsToSync(win));
  scoped_ptr<SessionTab> tab(new SessionTab());
  win.tabs.push_back(tab.release());
  ASSERT_TRUE(SessionModelAssociator::SessionWindowHasNoTabsToSync(win));
  TabNavigation nav(0, GURL("about:bubba"), GURL("about:referrer"),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), 0U);
  win.tabs[0]->navigations.push_back(nav);
  ASSERT_FALSE(SessionModelAssociator::SessionWindowHasNoTabsToSync(win));
}

TEST_F(SessionModelAssociatorTest, IsValidSessionTab) {
  SessionTab tab;
  ASSERT_FALSE(SessionModelAssociator::IsValidSessionTab(tab));
  TabNavigation nav(0, GURL(chrome::kChromeUINewTabURL),
                    GURL("about:referrer"),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), 0U);
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(SessionModelAssociator::IsValidSessionTab(tab));
  TabNavigation nav2(0, GURL("about:bubba"),
                    GURL("about:referrer"),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), 0U);
  tab.navigations.push_back(nav2);
  // Once there's another navigation, the tab is valid.
  ASSERT_TRUE(SessionModelAssociator::IsValidSessionTab(tab));
}

TEST_F(SessionModelAssociatorTest, IsValidSessionTabIgnoresFragmentForNtp) {
  SessionTab tab;
  ASSERT_FALSE(SessionModelAssociator::IsValidSessionTab(tab));
  TabNavigation nav(0, GURL(std::string(chrome::kChromeUINewTabURL) +
                            "#bookmarks"),
                    GURL("about:referrer"),
                    string16(ASCIIToUTF16("title")),
                    std::string("state"), 0U);
  tab.navigations.push_back(nav);
  // NewTab does not count as valid if it's the only navigation.
  ASSERT_FALSE(SessionModelAssociator::IsValidSessionTab(tab));
}

TEST_F(SessionModelAssociatorTest, PopulateSessionHeader) {
  sync_pb::SessionHeader header_s;
  header_s.set_client_name("Client 1");
  header_s.set_device_type(sync_pb::SessionHeader_DeviceType_TYPE_WIN);

  SyncedSession session;
  SessionModelAssociator::PopulateSessionHeaderFromSpecifics(
      header_s, &session);
  ASSERT_EQ("Client 1", session.session_name);
  ASSERT_EQ(SyncedSession::TYPE_WIN, session.device_type);
}

TEST_F(SessionModelAssociatorTest, PopulateSessionWindow) {
  sync_pb::SessionWindow window_s;
  window_s.add_tab(0);
  window_s.set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  window_s.set_selected_tab_index(1);

  std::string tag = "tag";
  SyncedSessionTracker tracker;
  SyncedSession* session = tracker.GetSession(tag);
  SessionWindow* win = new SessionWindow();
  session->windows.push_back(win);
  SessionModelAssociator::PopulateSessionWindowFromSpecifics(
      tag, window_s, base::Time(), win, &tracker);
  ASSERT_EQ(1U, win->tabs.size());
  ASSERT_EQ(1, win->selected_tab_index);
  ASSERT_EQ(1, win->type);
  ASSERT_EQ(1U, tracker.num_synced_sessions());
  ASSERT_EQ(1U, tracker.num_synced_tabs(std::string("tag")));

  // We do this so that when the destructor for the tracker is called, it will
  // be able to delete the session, window, and tab. We can't delete these
  // ourselves, otherwise we would run into double free errors when the
  // destructor was invoked (the true argument tells the tracker the tab
  // is now associated with a window).
  ASSERT_TRUE(tracker.GetSessionTab(tag, 0, true));
}

TEST_F(SessionModelAssociatorTest, PopulateSessionTab) {
  sync_pb::SessionTab tab_s;
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
  SessionModelAssociator::PopulateSessionTabFromSpecifics(
      tab_s, base::Time(), &tab);
  ASSERT_EQ(13, tab.tab_visual_index);
  ASSERT_EQ(3, tab.current_navigation_index);
  ASSERT_TRUE(tab.pinned);
  ASSERT_EQ("app_id", tab.extension_app_id);
  ASSERT_EQ(12, tab.navigations[0].index());
  ASSERT_EQ(GURL("referrer"), tab.navigations[0].referrer());
  ASSERT_EQ(string16(ASCIIToUTF16("title")), tab.navigations[0].title());
  ASSERT_EQ(PageTransition::TYPED, tab.navigations[0].transition());
  ASSERT_EQ(GURL("http://foo/1"), tab.navigations[0].virtual_url());
}

TEST_F(SessionModelAssociatorTest, SyncedSessionTracker) {
  const std::string tag1 = "tag";
  const std::string tag2 = "tag2";
  const std::string tag3 = "tag3";
  SyncedSessionTracker tracker;
  ASSERT_TRUE(tracker.empty());
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag1));
  SessionTab* tab = tracker.GetSessionTab(tag1, 0, false);
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  SessionTab* tab2 = tracker.GetSessionTab(tag1, 0, false);
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  ASSERT_EQ(tab, tab2);
  tab2 = tracker.GetSessionTab(tag2, 0, false);
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag2));
  ASSERT_EQ(0U, tracker.num_synced_sessions());

  ASSERT_FALSE(tracker.DeleteSession(tag1));
  ASSERT_FALSE(tracker.DeleteSession(tag3));

  SyncedSession* session = tracker.GetSession(tag1);
  SyncedSession* session2 = tracker.GetSession(tag2);
  SyncedSession* session3 = tracker.GetSession(tag3);
  ASSERT_EQ(3U, tracker.num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  ASSERT_TRUE(tracker.DeleteSession(tag3));
  ASSERT_EQ(2U, tracker.num_synced_sessions());

  const SessionTab *tab_ptr;
  ASSERT_TRUE(tracker.LookupSessionTab(tag1, 0, &tab_ptr));
  ASSERT_EQ(tab_ptr, tab);

  std::vector<SessionWindow*> windows;
  ASSERT_TRUE(tracker.LookupSessionWindows(tag1, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid windows, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(tracker.LookupAllForeignSessions(&sessions));

  tracker.clear();
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag2));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
}

#if defined(OS_WIN)
// The test is somewhat silly, and just verifies that we return a computer name.
TEST(SessionModelAssociatorTest, TestGetComputerName) {
  std::string computer_name = SessionModelAssociator::GetComputerName();
  EXPECT_TRUE(!computer_name.empty());
}
#endif

#if defined(OS_MACOSX)
// The test is somewhat silly, and just verifies that we return a hardware
// model name.
TEST_F(SessionModelAssociatorTest, GetHardwareModelName) {
  std::string hardware_model = SessionModelAssociator::GetHardwareModelName();
  EXPECT_TRUE(!hardware_model.empty());
}
#endif


}  // namespace browser_sync
