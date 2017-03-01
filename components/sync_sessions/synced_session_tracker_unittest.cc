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
const int kWindow1 = 1;
const int kTabNode = 0;
const int kTab1 = 15;
const int kTab2 = 25;
const int kTab3 = 35;

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override {}

  SyncedSessionTracker* GetTracker() { return &tracker_; }
  TabNodePool* GetTabNodePool() { return &tracker_.local_tab_pool_; }

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
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, 0);
  ASSERT_EQ(tab, GetTracker()->GetTab(kTag, 0));
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
  GetTracker()->PutTabInWindow(kTag, 10, 15);  // win id 10, tab id 15
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[10]->tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, 15),
            session->windows[10]->tabs[0].get());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  std::vector<const SyncedSession*> sessions;
  ASSERT_FALSE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 15);
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, 15);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  GetTracker()->GetSession(kTag2);
  GetTracker()->GetSession(kTag3);
  GetTracker()->PutWindowInSession(kTag3, 0);
  GetTracker()->PutTabInWindow(kTag3, 0, 15);
  tab = GetTracker()->GetTab(kTag3, 15);
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
  ASSERT_FALSE(
      GetTracker()->LookupSessionTab(kTag, TabNodePool::kInvalidTabID, &tab));
  ASSERT_FALSE(GetTracker()->LookupSessionTab(kTag, 5, &tab));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 5);
  ASSERT_TRUE(GetTracker()->LookupSessionTab(kTag, 5, &tab));
  ASSERT_NE((sessions::SessionTab*)nullptr, tab);
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  std::vector<sessions::SessionTab *> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(GetTracker()->Empty());
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  tabs1.push_back(GetTracker()->GetTab(kTag, 0));
  tabs1.push_back(GetTracker()->GetTab(kTag, 1));
  tabs1.push_back(GetTracker()->GetTab(kTag, 2));
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  temp_tab = GetTracker()->GetTab(kTag, 0);  // Already created.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(GetTracker()->GetTab(kTag2, 0));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_FALSE(GetTracker()->DeleteForeignSession(kTag3));

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
  ASSERT_TRUE(GetTracker()->DeleteForeignSession(kTag3));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());

  GetTracker()->PutWindowInSession(kTag, 0);           // Create a window.
  GetTracker()->PutTabInWindow(kTag, 0, 2);            // No longer unmapped.
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
      sessions::SessionTab* tab = GetTracker()->GetTab(tag, rand_tab_num + 1);
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupForeignTabNodeIds) {
  std::set<int> result;

  GetTracker()->OnTabNodeSeen(kTag, 1);
  GetTracker()->OnTabNodeSeen(kTag, 2);
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));
  GetTracker()->LookupForeignTabNodeIds(kTag2, &result);
  EXPECT_TRUE(result.empty());

  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 3);
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());

  GetTracker()->OnTabNodeSeen(kTag, 3);
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(3));

  GetTracker()->OnTabNodeSeen(kTag2, 21);
  GetTracker()->OnTabNodeSeen(kTag2, 22);
  GetTracker()->LookupForeignTabNodeIds(kTag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));

  GetTracker()->LookupForeignTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->PutWindowInSession(kTag3, 1);
  GetTracker()->PutTabInWindow(kTag3, 1, 5);
  GetTracker()->LookupForeignTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag3));
  GetTracker()->LookupForeignTabNodeIds(kTag3, &result);
  EXPECT_TRUE(result.empty());

  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag));
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->LookupForeignTabNodeIds(kTag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));

  GetTracker()->OnTabNodeSeen(kTag2, 21);
  GetTracker()->OnTabNodeSeen(kTag2, 23);
  GetTracker()->LookupForeignTabNodeIds(kTag2, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  EXPECT_FALSE(result.end() == result.find(23));

  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag2));
  GetTracker()->LookupForeignTabNodeIds(kTag2, &result);
  EXPECT_TRUE(result.empty());
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  ASSERT_TRUE(GetTracker()->Empty());

  // Create some session information that is stale.
  SyncedSession* session1 = GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 1);
  GetTracker()->GetTab(kTag, 2)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->GetTab(kTag, 3)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->PutWindowInSession(kTag, 1);
  GetTracker()->PutTabInWindow(kTag, 1, 4);
  GetTracker()->PutTabInWindow(kTag, 1, 5);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[0]->tabs.size());
  ASSERT_EQ(2U, session1->windows[1]->tabs.size());
  ASSERT_EQ(6U, GetTracker()->num_synced_tabs(kTag));

  // Create a session that should not be affected.
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, 2);
  GetTracker()->PutTabInWindow(kTag2, 2, 1);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  GetTracker()->GetTab(kTag, 6);
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, 0);
  GetTracker()->PutTabInWindow(kTag, 0, 0);
  // Tab 1 is closed.
  GetTracker()->PutTabInWindow(kTag, 0, 2);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  GetTracker()->PutTabInWindow(kTag, 0, 4);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  GetTracker()->PutTabInWindow(kTag, 0, 6);  // No longer unmapped.
  // Session 2 should not be affected.
  GetTracker()->CleanupForeignSession(kTag);

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
  int tab_node_id_1 = 1;
  int tab_node_id_2 = 2;
  std::set<int> result;

  GetTracker()->OnTabNodeSeen(kTag, tab_node_id_1);
  GetTracker()->OnTabNodeSeen(kTag, tab_node_id_2);

  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_1) != result.end());
  EXPECT_TRUE(result.find(tab_node_id_2) != result.end());

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_1);
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_EQ(1U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_2) != result.end());

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_2);
  GetTracker()->LookupForeignTabNodeIds(kTag, &result);
  EXPECT_TRUE(result.empty());
}

TEST_F(SyncedSessionTrackerTest, CleanupLocalTabs) {
  std::set<int> free_node_ids;
  int tab_node_id = TabNodePool::kInvalidTabNodeID;
  const int kTabNode1 = 1;
  const int kTabNode2 = 2;
  const int kTabNode3 = 3;

  GetTracker()->SetLocalSessionTag(kTag);

  // Start with two restored tab nodes.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  GetTracker()->ReassociateLocalTab(kTabNode2, kTab2);
  EXPECT_TRUE(GetTabNodePool()->Empty());
  EXPECT_FALSE(GetTabNodePool()->Full());
  EXPECT_EQ(2U, GetTabNodePool()->Capacity());

  // Associate with no tabs. The tab pool should now be full.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_TRUE(GetTabNodePool()->Full());

  // Associate with only 1 tab open. A tab node should be reused.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_TRUE(GetTracker()->GetTabNodeFromLocalTabId(kTab1, &tab_node_id));
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());

  // TabNodePool should have one free tab node and one used.
  EXPECT_EQ(2U, GetTabNodePool()->Capacity());
  EXPECT_FALSE(GetTabNodePool()->Empty());
  EXPECT_FALSE(GetTabNodePool()->Full());

  // Simulate a tab opening, which should use the last free tab node.
  EXPECT_TRUE(GetTracker()->GetTabNodeFromLocalTabId(kTab2, &tab_node_id));
  EXPECT_TRUE(GetTabNodePool()->Empty());

  // Simulate another tab opening, which should create a new associated tab
  // node.
  EXPECT_FALSE(GetTracker()->GetTabNodeFromLocalTabId(kTab3, &tab_node_id));
  EXPECT_EQ(kTabNode3, tab_node_id);
  EXPECT_EQ(3U, GetTabNodePool()->Capacity());
  EXPECT_TRUE(GetTabNodePool()->Empty());

  // Fetching the same tab should return the same tab node id.
  EXPECT_TRUE(GetTracker()->GetTabNodeFromLocalTabId(kTab3, &tab_node_id));
  EXPECT_EQ(kTabNode3, tab_node_id);
  EXPECT_TRUE(GetTabNodePool()->Empty());

  // Associate with no tabs. All tabs should be freed again, and the pool
  // should now be full.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_TRUE(GetTabNodePool()->Full());
  EXPECT_FALSE(GetTabNodePool()->Empty());
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMapped) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->SetLocalSessionTag(kTag);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  GetTracker()->ReassociateLocalTab(kTabNode, kTab1);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode, kTab2);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the new tab id into the window.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->tabs[0].get());
  ASSERT_EQ(session->tab_node_ids.size(),
            session->tab_node_ids.count(kTabNode));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMappedTwice) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->SetLocalSessionTag(kTag);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  GetTracker()->ReassociateLocalTab(kTabNode, kTab1);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->tabs.size());
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode, kTab2);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Tab 1 should no longer be associated with any SessionTab object. At this
  // point there's no need to verify it's unmapped state.
  const sessions::SessionTab* tab_ptr = nullptr;
  EXPECT_FALSE(GetTracker()->LookupSessionTab(kTag, kTab1, &tab_ptr));

  // Reset tracking and add back both the old tab and the new tab (both of which
  // refer to the same tab node id).
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->tabs[1].get());
  EXPECT_EQ(session->tab_node_ids.size(),
            session->tab_node_ids.count(kTabNode));
  EXPECT_EQ(1U, GetTabNodePool()->Capacity());

  // Attempting to access the original tab will create a new SessionTab object.
  EXPECT_NE(GetTracker()->GetTab(kTag, kTab1),
            GetTracker()->GetTab(kTag, kTab2));
  int tab_node_id = -1;
  EXPECT_FALSE(GetTracker()->GetTabNodeFromLocalTabId(kTab1, &tab_node_id));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabUnmapped) {
  std::set<int> free_node_ids;

  // First create the old tab in an unmapped state.
  GetTracker()->SetLocalSessionTag(kTag);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  GetTracker()->ReassociateLocalTab(kTabNode, kTab1);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window, but reassociated with a new tab id.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->ReassociateLocalTab(kTabNode, kTab2);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->tabs[0].get());
  ASSERT_EQ(session->tab_node_ids.size(),
            session->tab_node_ids.count(kTabNode));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
}

}  // namespace sync_sessions
