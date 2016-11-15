// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kInvalidUrl[] = "invalid.url";
const char kTag[] = "tag";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const char kTitle[] = "title";

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override {}

  SyncedSessionTracker* GetTracker() { return &tracker_; }

 private:
  FakeSyncSessionsClient sessions_client_;
  SyncedSessionTracker tracker_;
};

TEST_F(SyncedSessionTrackerTest, GetSession) {
  SyncedSession* session1 = GetTracker()->GetSession(kTag);
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  ASSERT_EQ(session1, GetTracker()->GetSession(kTag));
  ASSERT_NE(session1, session2);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, GetTabUnmapped) {
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, 0, 0);
  ASSERT_EQ(tab, GetTracker()->GetTab(kTag, 0, 0));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  GetTracker()->PutWindowInSession(kTag, 0);
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  GetTracker()->PutWindowInSession(kTag, 10);
  GetTracker()->PutTabInWindow(kTag, 10, 15,
                               0);  // win id 10, tab id 15, tab ind 0.
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[10]->tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, 15, 1),
            session->windows[10]->tabs[0].get());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 15, 0);
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, 15, 1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  GetTracker()->GetSession(kTag2);
  GetTracker()->GetSession(kTag3);
  GetTracker()->PutWindowInSession(kTag3, 0);
  GetTracker()->PutTabInWindow(kTag3, 0, 15, 0);
  tab = GetTracker()->GetTab(kTag3, 15, 1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kInvalidUrl, kTitle));
  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  // Only the session with a valid window and tab gets returned.
  ASSERT_EQ(1U, sessions.size());
  ASSERT_EQ(kTag, sessions[0]->session_tag);

  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW));
  ASSERT_EQ(3U, sessions.size());
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_FALSE(GetTracker()->LookupSessionWindows(kTag, &windows));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutWindowInSession(kTag, 2);
  GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, 0);
  GetTracker()->PutWindowInSession(kTag2, 2);
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(2U, windows.size());  // Only windows from kTag session.
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[0]);
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  const sessions::SessionTab* tab;
  ASSERT_FALSE(GetTracker()->LookupSessionTab(kTag, 5, &tab));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 5, 0);
  ASSERT_TRUE(GetTracker()->LookupSessionTab(kTag, 5, &tab));
  ASSERT_NE((sessions::SessionTab*)nullptr, tab);
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  std::vector<sessions::SessionTab *> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(GetTracker()->Empty());
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  tabs1.push_back(GetTracker()->GetTab(kTag, 0, 0));
  tabs1.push_back(GetTracker()->GetTab(kTag, 1, 1));
  tabs1.push_back(GetTracker()->GetTab(kTag, 2, 2));
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  temp_tab = GetTracker()->GetTab(kTag, 0, 0);  // Already created.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(GetTracker()->GetTab(kTag2, 0, 0));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_FALSE(GetTracker()->DeleteSession(kTag3));

  SyncedSession* session = GetTracker()->GetSession(kTag);
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  SyncedSession* session3 = GetTracker()->GetSession(kTag3);
  session3->device_type = SyncedSession::TYPE_OTHER;
  ASSERT_EQ(3U, GetTracker()->num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  ASSERT_TRUE(GetTracker()->DeleteSession(kTag3));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());

  GetTracker()->PutWindowInSession(kTag, 0);           // Create a window.
  GetTracker()->PutTabInWindow(kTag, 0, 2, 0);         // No longer unmapped.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));  // Has not changed.

  const sessions::SessionTab* tab_ptr;
  ASSERT_TRUE(GetTracker()->LookupSessionTab(kTag, 0, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[0]);
  ASSERT_TRUE(GetTracker()->LookupSessionTab(kTag, 2, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[2]);
  ASSERT_FALSE(GetTracker()->LookupSessionTab(kTag, 3, &tab_ptr));
  ASSERT_FALSE(tab_ptr);

  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag2, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW));
  ASSERT_EQ(2U, sessions.size());

  GetTracker()->Clear();
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
}

TEST_F(SyncedSessionTrackerTest, ManyGetTabs) {
  ASSERT_TRUE(GetTracker()->Empty());
  const int kMaxSessions = 10;
  const int kMaxTabs = 1000;
  const int kMaxAttempts = 10000;
  for (int j = 0; j < kMaxSessions; ++j) {
    std::string tag = base::StringPrintf("tag%d", j);
    for (int i = 0; i < kMaxAttempts; ++i) {
      // More attempts than tabs means we'll sometimes get the same tabs,
      // sometimes have to allocate new tabs.
      int rand_tab_num = base::RandInt(0, kMaxTabs);
      sessions::SessionTab* tab =
          GetTracker()->GetTab(tag, rand_tab_num, rand_tab_num + 1);
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupTabNodeIds) {
  std::set<int> result;

  GetTracker()->GetTab(kTag, 1, 1);
  GetTracker()->GetTab(kTag, 2, 2);
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));
  GetTracker()->LookupTabNodeIds(kTag2, &result);
  EXPECT_TRUE(result.empty());

  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 3, 0);
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());

  GetTracker()->GetTab(kTag, 3, 3);
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(3));

  GetTracker()->GetTab(kTag2, 1, 21);
  GetTracker()->GetTab(kTag2, 2, 22);
  GetTracker()->LookupTabNodeIds(kTag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));

  GetTracker()->LookupTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->PutWindowInSession(kTag3, 1);
  GetTracker()->PutTabInWindow(kTag3, 1, 5, 0);
  GetTracker()->LookupTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(GetTracker()->DeleteSession(kTag3));
  GetTracker()->LookupTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());

  EXPECT_FALSE(GetTracker()->DeleteSession(kTag));
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->LookupTabNodeIds(kTag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));

  GetTracker()->GetTab(kTag2, 1, 21);
  GetTracker()->GetTab(kTag2, 2, 23);
  GetTracker()->LookupTabNodeIds(kTag2, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  EXPECT_FALSE(result.end() == result.find(23));

  EXPECT_FALSE(GetTracker()->DeleteSession(kTag2));
  GetTracker()->LookupTabNodeIds(kTag2, &result);
  EXPECT_TRUE(result.empty());
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  ASSERT_TRUE(GetTracker()->Empty());

  // Create some session information that is stale.
  SyncedSession* session1 = GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 0, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 1, 1);
  GetTracker()->GetTab(kTag, 2, 3U)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->GetTab(kTag, 3, 4U)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->PutWindowInSession(kTag, 1);
  GetTracker()->PutTabInWindow(kTag, 1, 4, 0);
  GetTracker()->PutTabInWindow(kTag, 1, 5, 1);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[0]->tabs.size());
  ASSERT_EQ(2U, session1->windows[1]->tabs.size());
  ASSERT_EQ(6U, GetTracker()->num_synced_tabs(kTag));

  // Create a session that should not be affected.
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, 2);
  GetTracker()->PutTabInWindow(kTag2, 2, 1, 0);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  GetTracker()->GetTab(kTag, 6, 7U);
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 0, 0);
  // Tab 1 is closed.
  GetTracker()->PutTabInWindow(kTag, 0, 2, 1);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  GetTracker()->PutTabInWindow(kTag, 0, 4, 2);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  GetTracker()->PutTabInWindow(kTag, 0, 6, 3);  // No longer unmapped.
  // Session 2 should not be affected.
  GetTracker()->CleanupSession(kTag);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[0]->tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(4U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));

  // All memory should be properly deallocated by destructor for the
  // SyncedSessionTracker.
}

TEST_F(SyncedSessionTrackerTest, DeleteForeignTab) {
  int tab_id_1 = 1;
  int tab_id_2 = 2;
  int tab_node_id_3 = 3;
  int tab_node_id_4 = 4;
  std::set<int> result;

  GetTracker()->GetTab(kTag, tab_id_1, tab_node_id_3);
  GetTracker()->GetTab(kTag, tab_id_1, tab_node_id_4);
  GetTracker()->GetTab(kTag, tab_id_2, tab_node_id_3);
  GetTracker()->GetTab(kTag, tab_id_2, tab_node_id_4);

  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_3) != result.end());
  EXPECT_TRUE(result.find(tab_node_id_4) != result.end());

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_3);
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_EQ(1U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_4) != result.end());

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_4);
  GetTracker()->LookupTabNodeIds(kTag, &result);
  EXPECT_TRUE(result.empty());
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMapped) {
  const int kWindow1 = 1;
  const int kTab1 = 15;
  const int kTab2 = 25;
  const int kTabNodeId = 1;

  // First create the tab normally.
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1, 0);
  SyncedSession* session = GetTracker()->GetSession(kTag);
  sessions::SessionTab* tab = session->windows[kWindow1]->tabs[0].get();
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1, 1),
            session->windows[kWindow1]->tabs[0].get());

  // Then reassociate with a new tab id.
  ASSERT_TRUE(GetTracker()->ReassociateTab(kTag, kTab1, kTab2));
  ASSERT_EQ(tab, GetTracker()->GetTab(kTag, kTab2, kTabNodeId));

  // Reset tracking, and put the new tab id into the window.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2, 0);
  GetTracker()->CleanupSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2, kTabNodeId),
            session->windows[kWindow1]->tabs[0].get());
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabUnmapped) {
  const int kWindow1 = 1;
  const int kTab1 = 15;
  const int kTab2 = 25;
  const int kTabNodeId = 1;

  // First create the old tab in an unmapped state.
  ASSERT_TRUE(GetTracker()->GetTab(kTag, kTab1, kTabNodeId));

  // Then handle loading the window and reassociating the tab.
  GetTracker()->ResetSessionTracking(kTag);
  ASSERT_TRUE(GetTracker()->ReassociateTab(kTag, kTab1, kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2, 0);
  GetTracker()->CleanupSession(kTag);

  // Then reassociate with a new tab id. It should be accessible both via the
  // GetSession as well as GetTab.
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2, 1),
            session->windows[kWindow1]->tabs[0].get());
}

}  // namespace sync_sessions
