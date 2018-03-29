// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;

namespace sync_sessions {

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kSessionName[] = "sessionname";
const sync_pb::SyncEnums::DeviceType kDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
const char kTag[] = "tag";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const char kTitle[] = "title";
const int kTabNode1 = 1;
const int kTabNode2 = 2;
const int kTabNode3 = 3;
const SessionID kWindow1 = SessionID::FromSerializedValue(1);
const SessionID kWindow2 = SessionID::FromSerializedValue(2);
const SessionID kWindow3 = SessionID::FromSerializedValue(3);
const SessionID kTab1 = SessionID::FromSerializedValue(15);
const SessionID kTab2 = SessionID::FromSerializedValue(25);
const SessionID kTab3 = SessionID::FromSerializedValue(35);
const SessionID kTab4 = SessionID::FromSerializedValue(45);
const SessionID kTab5 = SessionID::FromSerializedValue(55);
const SessionID kTab6 = SessionID::FromSerializedValue(65);
const SessionID kTab7 = SessionID::FromSerializedValue(75);

MATCHER_P(HasSessionTag, expected_tag, "") {
  return arg->session_tag == expected_tag;
}

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override {}

  SyncedSessionTracker* GetTracker() { return &tracker_; }
  TabNodePool* GetTabNodePool() { return &tracker_.local_tab_pool_; }

  // Verify that each tab within a session is allocated one SessionTab object,
  // and that that tab object is owned either by the Session itself or the
  // |unmapped_tabs_| tab holder.
  AssertionResult VerifyTabIntegrity(const std::string& session_tag) {
    const SyncedSessionTracker::TrackedSession* session =
        tracker_.LookupTrackedSession(session_tag);
    if (!session) {
      return AssertionFailure()
             << "Not tracked session with tag " << session_tag;
    }

    // First get all the tabs associated with this session.
    int total_tab_count = session->synced_tab_map.size();

    // Now traverse the SyncedSession tree to verify the mapped tabs all match
    // up.
    int mapped_tab_count = 0;
    for (auto& window_pair : session->synced_session.windows) {
      mapped_tab_count += window_pair.second->wrapped_window.tabs.size();
      for (auto& tab : window_pair.second->wrapped_window.tabs) {
        const auto tab_map_it = session->synced_tab_map.find(tab->tab_id);
        if (tab_map_it == session->synced_tab_map.end()) {
          return AssertionFailure() << "Tab ID " << tab->tab_id.id()
                                    << " has no corresponding synced tab entry";
        }
        if (tab_map_it->second != tab.get()) {
          return AssertionFailure()
                 << "Mapped tab " << tab->tab_id.id()
                 << " does not match synced tab map " << tab->tab_id.id();
        }
      }
    }

    // Wrap up by verifying all unmapped tabs are tracked.
    int unmapped_tab_count = session->unmapped_tabs.size();
    for (const auto& tab_pair : session->unmapped_tabs) {
      if (tab_pair.first != tab_pair.second->tab_id) {
        return AssertionFailure()
               << "Unmapped tab " << tab_pair.second->tab_id.id()
               << " associated with wrong tab " << tab_pair.first;
      }
      const auto tab_map_it =
          session->synced_tab_map.find(tab_pair.second->tab_id);
      if (tab_map_it == session->synced_tab_map.end()) {
        return AssertionFailure() << "Unmapped tab " << tab_pair.second->tab_id
                                  << " has no corresponding synced tab entry";
      }
      if (tab_map_it->second != tab_pair.second.get()) {
        return AssertionFailure()
               << "Unmapped tab " << tab_pair.second->tab_id.id()
               << " does not match synced tab map " << tab_map_it->second;
      }
    }

    return mapped_tab_count + unmapped_tab_count == total_tab_count
               ? AssertionSuccess()
               : AssertionFailure()
                     << " Tab count mismatch. Total: " << total_tab_count
                     << ". Mapped + Unmapped: " << mapped_tab_count << " + "
                     << unmapped_tab_count;
  }

  MockSyncSessionsClient* GetSyncSessionsClient() { return &sessions_client_; }

 private:
  testing::NiceMock<MockSyncSessionsClient> sessions_client_;
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
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_EQ(tab, GetTracker()->GetTab(kTag, kTab1));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());

  // Doing it again should have no effect.
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  const char kInvalidUrl[] = "invalid.url";
  ON_CALL(*GetSyncSessionsClient(), ShouldSyncURL(GURL(kInvalidUrl)))
      .WillByDefault(testing::Return(false));

  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  GetTracker()->GetSession(kTag2);
  GetTracker()->GetSession(kTag3);
  GetTracker()->PutWindowInSession(kTag3, kWindow1);
  GetTracker()->PutTabInWindow(kTag3, kWindow1, kTab1);
  tab = GetTracker()->GetTab(kTag3, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kInvalidUrl, kTitle));
  // Only the session with a valid window and tab gets returned.
  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      ElementsAre(HasSessionTag(kTag)));
  EXPECT_THAT(GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2),
                          HasSessionTag(kTag3)));
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_FALSE(GetTracker()->LookupSessionWindows(kTag, &windows));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutWindowInSession(kTag, kWindow2);
  GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, kWindow1);
  GetTracker()->PutWindowInSession(kTag2, kWindow2);
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(2U, windows.size());  // Only windows from kTag session.
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[0]);
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, SessionID::InvalidValue()),
              IsNull());
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), IsNull());
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), NotNull());
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  std::vector<sessions::SessionTab *> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(GetTracker()->Empty());
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab1));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab2));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab3));
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  temp_tab = GetTracker()->GetTab(kTag, kTab1);  // Already created.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(GetTracker()->GetTab(kTag2, kTab1));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_FALSE(GetTracker()->DeleteForeignSession(kTag3));

  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  SyncedSession* session3 = GetTracker()->GetSession(kTag3);
  session3->device_type = sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
  ASSERT_EQ(3U, GetTracker()->num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  ASSERT_TRUE(GetTracker()->DeleteForeignSession(kTag3));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());

  GetTracker()->PutWindowInSession(kTag, kWindow1);     // Create a window.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));  // Has not changed.

  ASSERT_EQ(tabs1[0], GetTracker()->LookupSessionTab(kTag, kTab1));
  ASSERT_EQ(tabs1[2], GetTracker()->LookupSessionTab(kTag, kTab3));
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab4), IsNull());

  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag2, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  EXPECT_THAT(GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2)));

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
      sessions::SessionTab* tab = GetTracker()->GetTab(
          tag, SessionID::FromSerializedValue(rand_tab_num + 1));
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

  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
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
  GetTracker()->PutWindowInSession(kTag3, kWindow2);
  GetTracker()->PutTabInWindow(kTag3, kWindow2, kTab2);
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
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->GetTab(kTag, kTab3)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  GetTracker()->GetTab(kTag, kTab4)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  GetTracker()->PutWindowInSession(kTag, kWindow2);
  GetTracker()->PutTabInWindow(kTag, kWindow2, kTab5);
  GetTracker()->PutTabInWindow(kTag, kWindow2, kTab6);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, session1->windows[kWindow2]->wrapped_window.tabs.size());
  ASSERT_EQ(6U, GetTracker()->num_synced_tabs(kTag));

  // Create a session that should not be affected.
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, kWindow3);
  GetTracker()->PutTabInWindow(kTag2, kWindow3, kTab2);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  GetTracker()->GetTab(kTag, kTab7);
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  // Tab 1 is closed.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab5);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab7);  // No longer unmapped.
  // Session 2 should not be affected.
  GetTracker()->CleanupSession(kTag);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(4U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));

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
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, CleanupLocalTabs) {
  std::set<int> free_node_ids;
  int tab_node_id = TabNodePool::kInvalidTabNodeID;

  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);

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
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMapped) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the new tab id into the window.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->GetTabNodeIdsForTesting(kTag).size(),
            GetTracker()->GetTabNodeIdsForTesting(kTag).count(kTabNode1));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMappedTwice) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->wrapped_window.tabs.size());
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Tab 1 should no longer be associated with any SessionTab object. At this
  // point there's no need to verify it's unmapped state.
  EXPECT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), IsNull());

  // Reset tracking and add back both the old tab and the new tab (both of which
  // refer to the same tab node id).
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->wrapped_window.tabs[1].get());
  EXPECT_EQ(GetTracker()->GetTabNodeIdsForTesting(kTag).size(),
            GetTracker()->GetTabNodeIdsForTesting(kTag).count(kTabNode1));
  EXPECT_EQ(1U, GetTabNodePool()->Capacity());

  // Attempting to access the original tab will create a new SessionTab object.
  EXPECT_NE(GetTracker()->GetTab(kTag, kTab1),
            GetTracker()->GetTab(kTag, kTab2));
  int tab_node_id = -1;
  EXPECT_FALSE(GetTracker()->GetTabNodeFromLocalTabId(kTab1, &tab_node_id));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabUnmapped) {
  std::set<int> free_node_ids;

  // First create the old tab in an unmapped state.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window, but reassociated with a new tab id.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->GetTabNodeIdsForTesting(kTag).size(),
            GetTracker()->GetTabNodeIdsForTesting(kTag).count(kTabNode1));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldUnmappedNewMapped) {
  std::set<int> free_node_ids;

  // First create the old tab in an unmapped state.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map an unseen tab to a window, then reassociate the existing tab to the
  // mapped tab id.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->GetTabNodeIdsForTesting(kTag).size(),
            GetTracker()->GetTabNodeIdsForTesting(kTag).count(kTabNode1));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabSameTabId) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());

  // Reassociate, using the same tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the tab id back into the same window.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->GetTabNodeIdsForTesting(kTag).size(),
            GetTracker()->GetTabNodeIdsForTesting(kTag).count(kTabNode1));
  ASSERT_EQ(1U, GetTabNodePool()->Capacity());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldMappedNewUnmapped) {
  std::set<int> free_node_ids;

  // First create an unmapped tab.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Now, map the first one, deleting the second one.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());

  // Create a second unmapped tab.
  GetTracker()->ReassociateLocalTab(kTabNode2, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Reassociate the second tab with node of the first tab.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(GetTracker()->IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now map the new one.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows[kWindow1]->wrapped_window.tabs[0].get());
  ASSERT_EQ(2U, GetTabNodePool()->Capacity());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

}  // namespace sync_sessions
