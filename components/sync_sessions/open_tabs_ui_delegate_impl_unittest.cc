// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"

#include <vector>

#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sessions::SessionTab;
using testing::ElementsAre;
using testing::Field;
using testing::Pointee;
using testing::Property;

const char kSessionTag1[] = "foreign1";
const char kSessionTag2[] = "foreign2";
const char kSessionTag3[] = "foreign3";
const SessionID::id_type kWindowId1 = 1;
const SessionID::id_type kWindowId2 = 2;
const SessionID::id_type kWindowId3 = 3;
const SessionID::id_type kTabId1 = 111;
const SessionID::id_type kTabId2 = 222;
const SessionID::id_type kTabId3 = 333;

void IngnoreForeignSessionDeletion(const std::string& session_tag) {}

class OpenTabsUIDelegateImplTest : public testing::Test {
 protected:
  OpenTabsUIDelegateImplTest()
      : session_tracker_(&mock_sync_sessions_client_),
        delegate_(&mock_sync_sessions_client_,
                  &session_tracker_,
                  /*favicon_cache=*/nullptr,
                  base::BindRepeating(&IngnoreForeignSessionDeletion)) {}

  testing::NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
  SyncedSessionTracker session_tracker_;
  OpenTabsUIDelegateImpl delegate_;
};

TEST_F(OpenTabsUIDelegateImplTest, ShouldSortSessions) {
  const base::Time kTime0 = base::Time::Now();

  // Create three sessions, with one window and tab each.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url1", "title1"));
  session_tracker_.GetSession(kSessionTag1)->modified_time =
      kTime0 + base::TimeDelta::FromSeconds(3);

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url2", "title2"));
  session_tracker_.GetSession(kSessionTag2)->modified_time =
      kTime0 + base::TimeDelta::FromSeconds(1);

  session_tracker_.PutWindowInSession(kSessionTag3, kWindowId3);
  session_tracker_.PutTabInWindow(kSessionTag3, kWindowId3, kTabId3);
  session_tracker_.GetTab(kSessionTag3, kTabId3)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url3", "title3"));
  session_tracker_.GetSession(kSessionTag3)->modified_time =
      kTime0 + base::TimeDelta::FromSeconds(2);

  std::vector<const SyncedSession*> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_THAT(sessions,
              ElementsAre(Field(&SyncedSession::session_tag, kSessionTag1),
                          Field(&SyncedSession::session_tag, kSessionTag3),
                          Field(&SyncedSession::session_tag, kSessionTag2)));
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSortTabs) {
  const base::Time kTime0 = base::Time::Now();
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId2, kTabId2);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId2, kTabId3);

  sessions::SessionTab* tab1 = session_tracker_.GetTab(kSessionTag1, kTabId1);
  tab1->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://url1", "title1"));
  tab1->timestamp = kTime0 + base::TimeDelta::FromSeconds(3);

  sessions::SessionTab* tab2 = session_tracker_.GetTab(kSessionTag1, kTabId2);
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://url1", "title1"));
  tab2->timestamp = kTime0 + base::TimeDelta::FromSeconds(1);

  sessions::SessionTab* tab3 = session_tracker_.GetTab(kSessionTag1, kTabId3);
  tab3->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://url1", "title1"));
  tab3->timestamp = kTime0 + base::TimeDelta::FromSeconds(2);

  std::vector<const SessionTab*> tabs;
  EXPECT_TRUE(delegate_.GetForeignSessionTabs(kSessionTag1, &tabs));
  EXPECT_THAT(tabs,
              ElementsAre(Pointee(Field(&SessionTab::tab_id,
                                        Property(&SessionID::id, kTabId1))),
                          Pointee(Field(&SessionTab::tab_id,
                                        Property(&SessionID::id, kTabId3))),
                          Pointee(Field(&SessionTab::tab_id,
                                        Property(&SessionID::id, kTabId2)))));
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSkipNonPresentable) {
  // Create two sessions, with one window and tab each, but only the second
  // contains a navigation.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1);

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url2", "title2"));

  std::vector<const SyncedSession*> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_THAT(sessions,
              ElementsAre(Field(&SyncedSession::session_tag, kSessionTag2)));
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSkipNonSyncableTabs) {
  ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL("http://url1")))
      .WillByDefault(testing::Return(false));

  // Create two sessions, with one window and tab each. The first of the two
  // contains a URL that should not be synced.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url1", "title1"));

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
              "http://url2", "title2"));

  std::vector<const SyncedSession*> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_THAT(sessions,
              ElementsAre(Field(&SyncedSession::session_tag, kSessionTag2)));
}

}  // namespace
}  // namespace sync_sessions
