// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/synced_session_tracker.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

typedef testing::Test SyncedSessionTrackerTest;

TEST_F(SyncedSessionTrackerTest, GetSession) {
  SyncedSessionTracker tracker;
  SyncedSession* session1 = tracker.GetSession("tag");
  SyncedSession* session2 = tracker.GetSession("tag2");
  ASSERT_EQ(session1, tracker.GetSession("tag"));
  ASSERT_NE(session1, session2);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, GetTabUnmapped) {
  SyncedSessionTracker tracker;
  SessionTab* tab = tracker.GetTab("tag", 0, 0);
  ASSERT_EQ(tab, tracker.GetTab("tag", 0, 0));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  SyncedSessionTracker tracker;
  tracker.PutWindowInSession("tag", 0);
  SyncedSession* session = tracker.GetSession("tag");
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  SyncedSessionTracker tracker;
  tracker.PutWindowInSession("tag", 10);
  tracker.PutTabInWindow("tag", 10, 15, 0);  // win id 10, tab id 15, tab ind 0.
  SyncedSession* session = tracker.GetSession("tag");
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[10]->tabs.size());
  ASSERT_EQ(tracker.GetTab("tag", 15, 1), session->windows[10]->tabs[0]);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  SyncedSessionTracker tracker;
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(tracker.LookupAllForeignSessions(&sessions));
  tracker.GetSession("tag1");
  tracker.GetSession("tag2");
  tracker.PutWindowInSession("tag1", 0);
  tracker.PutTabInWindow("tag1", 0, 15, 0);
  SessionTab* tab = tracker.GetTab("tag1", 15, 1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
      "bla://valid_url", "title"));
  ASSERT_TRUE(tracker.LookupAllForeignSessions(&sessions));
  // Only the session with a valid window and tab gets returned.
  ASSERT_EQ(1U, sessions.size());
  ASSERT_EQ("tag1", sessions[0]->session_tag);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  SyncedSessionTracker tracker;
  std::vector<const SessionWindow*> windows;
  ASSERT_FALSE(tracker.LookupSessionWindows("tag1", &windows));
  tracker.GetSession("tag1");
  tracker.PutWindowInSession("tag1", 0);
  tracker.PutWindowInSession("tag1", 2);
  tracker.GetSession("tag2");
  tracker.PutWindowInSession("tag2", 0);
  tracker.PutWindowInSession("tag2", 2);
  ASSERT_TRUE(tracker.LookupSessionWindows("tag1", &windows));
  ASSERT_EQ(2U, windows.size());  // Only windows from tag1 session.
  ASSERT_NE((SessionWindow*)NULL, windows[0]);
  ASSERT_NE((SessionWindow*)NULL, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  SyncedSessionTracker tracker;
  const SessionTab* tab;
  ASSERT_FALSE(tracker.LookupSessionTab("tag1", 5, &tab));
  tracker.GetSession("tag1");
  tracker.PutWindowInSession("tag1", 0);
  tracker.PutTabInWindow("tag1", 0, 5, 0);
  ASSERT_TRUE(tracker.LookupSessionTab("tag1", 5, &tab));
  ASSERT_NE((SessionTab*)NULL, tab);
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  const std::string tag1 = "tag";
  const std::string tag2 = "tag2";
  const std::string tag3 = "tag3";
  SyncedSessionTracker tracker;
  std::vector<SessionTab*> tabs1, tabs2;
  SessionTab* temp_tab;
  ASSERT_TRUE(tracker.Empty());
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag1));
  tabs1.push_back(tracker.GetTab(tag1, 0, 0));
  tabs1.push_back(tracker.GetTab(tag1, 1, 1));
  tabs1.push_back(tracker.GetTab(tag1, 2, 2));
  ASSERT_EQ(3U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  temp_tab = tracker.GetTab(tag1, 0, 0);  // Already created.
  ASSERT_EQ(3U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(tracker.GetTab(tag2, 0, 0));
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag2));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
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

  tracker.PutWindowInSession(tag1, 0);      // Create a window.
  tracker.PutTabInWindow(tag1, 0, 2, 0);    // No longer unmapped.
  ASSERT_EQ(3U, tracker.num_synced_tabs(tag1));      // Has not changed.

  const SessionTab *tab_ptr;
  ASSERT_TRUE(tracker.LookupSessionTab(tag1, 0, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[0]);
  ASSERT_TRUE(tracker.LookupSessionTab(tag1, 2, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[2]);
  ASSERT_FALSE(tracker.LookupSessionTab(tag1, 3, &tab_ptr));
  ASSERT_EQ(static_cast<const SessionTab*>(NULL), tab_ptr);

  std::vector<const SessionWindow*> windows;
  ASSERT_TRUE(tracker.LookupSessionWindows(tag1, &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(tracker.LookupSessionWindows(tag2, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(tracker.LookupAllForeignSessions(&sessions));

  tracker.Clear();
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(0U, tracker.num_synced_tabs(tag2));
  ASSERT_EQ(0U, tracker.num_synced_sessions());
}

TEST_F(SyncedSessionTrackerTest, ManyGetTabs) {
  SyncedSessionTracker tracker;
  ASSERT_TRUE(tracker.Empty());
  const int kMaxSessions = 10;
  const int kMaxTabs = 1000;
  const int kMaxAttempts = 10000;
  for (int j=0; j<kMaxSessions; ++j) {
    std::string tag = base::StringPrintf("tag%d", j);
    for (int i=0; i<kMaxAttempts; ++i) {
      // More attempts than tabs means we'll sometimes get the same tabs,
      // sometimes have to allocate new tabs.
      int rand_tab_num = base::RandInt(0, kMaxTabs);
      SessionTab* tab = tracker.GetTab(tag, rand_tab_num, rand_tab_num + 1);
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupTabNodeIds) {
  SyncedSessionTracker tracker;
  std::set<int> result;
  std::string tag1 = "session1";
  std::string tag2 = "session2";
  std::string tag3 = "session3";

  tracker.GetTab(tag1, 1, 1);
  tracker.GetTab(tag1, 2, 2);
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag1, &result));
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));
  EXPECT_FALSE(tracker.LookupTabNodeIds(tag2, &result));

  tracker.PutWindowInSession(tag1, 0);
  tracker.PutTabInWindow(tag1, 0, 3, 0);
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag1, &result));
  EXPECT_EQ(2U, result.size());

  tracker.GetTab(tag1, 3, 3);
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag1, &result));
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(3));

  tracker.GetTab(tag2, 1, 21);
  tracker.GetTab(tag2, 2, 22);
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag2, &result));
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag1, &result));
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));

  EXPECT_FALSE(tracker.LookupTabNodeIds(tag3, &result));
  tracker.PutWindowInSession(tag3, 1);
  tracker.PutTabInWindow(tag3, 1, 5, 0);
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag3, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_TRUE(tracker.DeleteSession(tag3));
  EXPECT_FALSE(tracker.LookupTabNodeIds(tag3, &result));

  EXPECT_TRUE(tracker.DeleteSession(tag1));
  EXPECT_FALSE(tracker.LookupTabNodeIds(tag1, &result));
  EXPECT_TRUE(tracker.LookupTabNodeIds(tag2, &result));
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  EXPECT_TRUE(tracker.DeleteSession(tag2));
  EXPECT_FALSE(tracker.LookupTabNodeIds(tag2, &result));
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  SyncedSessionTracker tracker;
  ASSERT_TRUE(tracker.Empty());
  std::string tag1 = "tag1";
  std::string tag2 = "tag2";

  // Create some session information that is stale.
  SyncedSession* session1= tracker.GetSession(tag1);
  tracker.PutWindowInSession(tag1, 0);
  tracker.PutTabInWindow(tag1, 0, 0, 0);
  tracker.PutTabInWindow(tag1, 0, 1, 1);
  tracker.GetTab(tag1, 2, 3U)->window_id.set_id(0);  // Will be unmapped.
  tracker.GetTab(tag1, 3, 4U)->window_id.set_id(0);  // Will be unmapped.
  tracker.PutWindowInSession(tag1, 1);
  tracker.PutTabInWindow(tag1, 1, 4, 0);
  tracker.PutTabInWindow(tag1, 1, 5, 1);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[0]->tabs.size());
  ASSERT_EQ(2U, session1->windows[1]->tabs.size());
  ASSERT_EQ(6U, tracker.num_synced_tabs(tag1));

  // Create a session that should not be affected.
  SyncedSession* session2 = tracker.GetSession(tag2);
  tracker.PutWindowInSession(tag2, 2);
  tracker.PutTabInWindow(tag2, 2, 1, 0);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  tracker.GetTab(tag1, 6, 7U);
  tracker.ResetSessionTracking(tag1);
  tracker.PutWindowInSession(tag1, 0);
  tracker.PutTabInWindow(tag1, 0, 0, 0);
  // Tab 1 is closed.
  tracker.PutTabInWindow(tag1, 0, 2, 1);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  tracker.PutTabInWindow(tag1, 0, 4, 2);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  tracker.PutTabInWindow(tag1, 0, 6, 3);  // No longer unmapped.
  // Session 2 should not be affected.
  tracker.CleanupSession(tag1);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[0]->tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(2U, tracker.num_synced_sessions());
  ASSERT_EQ(4U, tracker.num_synced_tabs(tag1));
  ASSERT_EQ(1U, tracker.num_synced_tabs(tag2));

  // All memory should be properly deallocated by destructor for the
  // SyncedSessionTracker.
}

}  // namespace browser_sync
