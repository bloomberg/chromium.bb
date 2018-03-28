// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/local_session_event_handler_impl.h"

#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/test_synced_window_delegates_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;
using testing::ByMove;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Return;
using testing::SizeIs;
using testing::StrictMock;
using testing::_;

const char kFoo1[] = "http://foo1/";
const char kBar1[] = "http://bar1/";
const char kBar2[] = "http://bar2/";
const char kBaz1[] = "http://baz1/";

const char kSessionTag[] = "sessiontag1";
const char kSessionName[] = "Session Name 1";

const base::Time kTime0 = base::Time::FromInternalValue(100);
const base::Time kTime1 = base::Time::FromInternalValue(110);
const base::Time kTime2 = base::Time::FromInternalValue(120);
const base::Time kTime3 = base::Time::FromInternalValue(130);
const base::Time kTime4 = base::Time::FromInternalValue(140);
const base::Time kTime5 = base::Time::FromInternalValue(150);
const base::Time kTime6 = base::Time::FromInternalValue(190);

const SessionID kWindowId1 = SessionID::FromSerializedValue(1000001);
const SessionID kWindowId2 = SessionID::FromSerializedValue(1000002);
const SessionID kTabId1 = SessionID::FromSerializedValue(1000003);
const SessionID kTabId2 = SessionID::FromSerializedValue(1000004);
const SessionID kTabId3 = SessionID::FromSerializedValue(1000005);

MATCHER_P3(MatchesHeader, session_tag, num_windows, num_tabs, "") {
  if (arg == nullptr) {
    *result_listener << "which is null";
    return false;
  }

  const sync_pb::SessionSpecifics& specifics = *arg;
  if (!specifics.has_header()) {
    *result_listener << "which is not a header entity";
    return false;
  }
  if (specifics.session_tag() != session_tag) {
    *result_listener << "which contains an unexpected session tag";
    return false;
  }
  if (specifics.header().window_size() != num_windows) {
    *result_listener << "which contains an unexpected number of windows: "
                     << specifics.header().window_size();
    return false;
  }
  int tab_count = 0;
  for (auto& window : specifics.header().window()) {
    tab_count += window.tab_size();
  }
  if (tab_count != num_tabs) {
    *result_listener << "which contains an unexpected number of tabs: "
                     << tab_count;
    return false;
  }
  return true;
}

MATCHER_P4(MatchesTab, session_tag, window_id, tab_id, urls, "") {
  if (arg == nullptr) {
    *result_listener << "which is null";
    return false;
  }

  const sync_pb::SessionSpecifics& specifics = *arg;

  if (!specifics.has_tab()) {
    *result_listener << "which is not a tab entity";
    return false;
  }
  if (specifics.session_tag() != session_tag) {
    *result_listener << "which contains an unexpected session tag";
    return false;
  }
  if (specifics.tab().window_id() != window_id.id()) {
    *result_listener << "which contains an unexpected window ID";
    return false;
  }
  if (specifics.tab().tab_id() != tab_id.id()) {
    *result_listener << "which contains an unexpected tab ID";
    return false;
  }
  if (specifics.tab().navigation_size() != static_cast<int>(urls.size())) {
    *result_listener << "which contains an unexpected number of windows";
    return false;
  }
  for (int i = 0; i < specifics.tab().navigation_size(); ++i) {
    if (specifics.tab().navigation(i).virtual_url() != urls.at(i)) {
      *result_listener << "which contains an unexpected navigation URL " << i;
      return false;
    }
  }
  return true;
}

class MockWriteBatch : public LocalSessionEventHandlerImpl::WriteBatch {
 public:
  MockWriteBatch() {}
  ~MockWriteBatch() override {}

  void Add(std::unique_ptr<sync_pb::SessionSpecifics> specifics) override {
    DoAdd(specifics.get());
  }

  void Update(std::unique_ptr<sync_pb::SessionSpecifics> specifics) override {
    DoUpdate(specifics.get());
  }

  MOCK_METHOD1(Delete, void(int tab_node_id));
  // TODO(crbug.com/729950): Use unique_ptr here direclty once move-only
  // arguments are supported in gMock.
  MOCK_METHOD1(DoAdd, void(sync_pb::SessionSpecifics* specifics));
  MOCK_METHOD1(DoUpdate, void(sync_pb::SessionSpecifics* specifics));
  MOCK_METHOD0(Commit, void());
};

class MockDelegate : public LocalSessionEventHandlerImpl::Delegate {
 public:
  ~MockDelegate() override {}

  MOCK_METHOD0(CreateLocalSessionWriteBatch,
               std::unique_ptr<LocalSessionEventHandlerImpl::WriteBatch>());
  MOCK_METHOD2(TrackLocalNavigationId,
               void(base::Time timestamp, int unique_id));
  MOCK_METHOD1(OnPageFaviconUpdated, void(const GURL& page_url));
  MOCK_METHOD2(OnFaviconVisited,
               void(const GURL& page_url, const GURL& favicon_url));
};

class LocalSessionEventHandlerImplTest : public testing::Test {
 public:
  LocalSessionEventHandlerImplTest()
      : session_tracker_(&mock_sync_sessions_client_) {
    ON_CALL(mock_sync_sessions_client_, GetSyncedWindowDelegatesGetter())
        .WillByDefault(testing::Return(&window_getter_));

    session_tracker_.InitLocalSession(kSessionTag, kSessionName,
                                      sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
  }

  void InitHandler(LocalSessionEventHandlerImpl::WriteBatch* initial_batch) {
    handler_ = std::make_unique<LocalSessionEventHandlerImpl>(
        &mock_delegate_, &mock_sync_sessions_client_, &session_tracker_,
        initial_batch);
    window_getter_.router()->StartRoutingTo(handler_.get());
  }

  void InitHandler() {
    NiceMock<MockWriteBatch> initial_batch;
    InitHandler(&initial_batch);
  }

  TestSyncedWindowDelegate* AddWindow(
      SessionID window_id = SessionID::NewUnique()) {
    return window_getter_.AddWindow(
        sync_pb::SessionWindow_BrowserType_TYPE_TABBED, window_id);
  }

  TestSyncedTabDelegate* AddTab(SessionID window_id,
                                const std::string& url,
                                SessionID tab_id = SessionID::NewUnique()) {
    TestSyncedTabDelegate* tab = window_getter_.AddTab(window_id, tab_id);
    tab->Navigate(url, base::Time::Now());
    return tab;
  }

  TestSyncedTabDelegate* AddTabWithTime(SessionID window_id,
                                        const std::string& url,
                                        base::Time time = base::Time::Now()) {
    TestSyncedTabDelegate* tab = window_getter_.AddTab(window_id);
    tab->Navigate(url, time);
    return tab;
  }

  testing::NiceMock<MockDelegate> mock_delegate_;
  testing::NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
  SyncedSessionTracker session_tracker_;
  TestSyncedWindowDelegatesGetter window_getter_;
  std::unique_ptr<LocalSessionEventHandlerImpl> handler_;
};

// Populate the mock tab delegate with some data and navigation
// entries and make sure that setting a SessionTab from it preserves
// those entries (and clobbers any existing data).
TEST_F(LocalSessionEventHandlerImplTest, SetSessionTabFromDelegate) {
  // Create a tab with three valid entries.
  TestSyncedTabDelegate* tab =
      AddTabWithTime(AddWindow()->GetSessionId(), kFoo1, kTime1);
  tab->Navigate(kBar1, kTime2);
  tab->Navigate(kBaz1, kTime3);

  sessions::SessionTab session_tab;
  session_tab.window_id.set_id(1);
  session_tab.tab_id.set_id(1);
  session_tab.tab_visual_index = 1;
  session_tab.current_navigation_index = 1;
  session_tab.pinned = true;
  session_tab.extension_app_id = "app id";
  session_tab.user_agent_override = "override";
  session_tab.timestamp = kTime5;
  session_tab.navigations.push_back(
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://www.example.com", "Example"));
  session_tab.session_storage_persistent_id = "persistent id";

  InitHandler();
  handler_->SetSessionTabFromDelegateForTest(*tab, kTime4, &session_tab);

  EXPECT_EQ(tab->GetWindowId(), session_tab.window_id);
  EXPECT_EQ(tab->GetSessionId(), session_tab.tab_id);
  EXPECT_EQ(0, session_tab.tab_visual_index);
  EXPECT_EQ(tab->GetCurrentEntryIndex(), session_tab.current_navigation_index);
  EXPECT_FALSE(session_tab.pinned);
  EXPECT_TRUE(session_tab.extension_app_id.empty());
  EXPECT_TRUE(session_tab.user_agent_override.empty());
  EXPECT_EQ(kTime4, session_tab.timestamp);
  ASSERT_EQ(3u, session_tab.navigations.size());
  EXPECT_EQ(GURL(kFoo1), session_tab.navigations[0].virtual_url());
  EXPECT_EQ(GURL(kBar1), session_tab.navigations[1].virtual_url());
  EXPECT_EQ(GURL(kBaz1), session_tab.navigations[2].virtual_url());
  EXPECT_EQ(kTime1, session_tab.navigations[0].timestamp());
  EXPECT_EQ(kTime2, session_tab.navigations[1].timestamp());
  EXPECT_EQ(kTime3, session_tab.navigations[2].timestamp());
  EXPECT_EQ(200, session_tab.navigations[0].http_status_code());
  EXPECT_EQ(200, session_tab.navigations[1].http_status_code());
  EXPECT_EQ(200, session_tab.navigations[2].http_status_code());
  EXPECT_EQ(SerializedNavigationEntry::STATE_INVALID,
            session_tab.navigations[0].blocked_state());
  EXPECT_EQ(SerializedNavigationEntry::STATE_INVALID,
            session_tab.navigations[1].blocked_state());
  EXPECT_EQ(SerializedNavigationEntry::STATE_INVALID,
            session_tab.navigations[2].blocked_state());
  EXPECT_TRUE(session_tab.session_storage_persistent_id.empty());
}

// Ensure the current_navigation_index gets set properly when the navigation
// stack gets trucated to +/- 6 entries.
TEST_F(LocalSessionEventHandlerImplTest,
       SetSessionTabFromDelegateNavigationIndex) {
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  const int kNavs = 10;
  for (int i = 1; i < kNavs; ++i) {
    tab->Navigate(base::StringPrintf("http://foo%i", i));
  }
  tab->set_current_entry_index(kNavs - 2);

  InitHandler();

  sessions::SessionTab session_tab;
  handler_->SetSessionTabFromDelegateForTest(*tab, kTime6, &session_tab);

  EXPECT_EQ(6, session_tab.current_navigation_index);
  ASSERT_EQ(8u, session_tab.navigations.size());
  EXPECT_EQ(GURL("http://foo2"), session_tab.navigations[0].virtual_url());
  EXPECT_EQ(GURL("http://foo3"), session_tab.navigations[1].virtual_url());
  EXPECT_EQ(GURL("http://foo4"), session_tab.navigations[2].virtual_url());
}

// Ensure the current_navigation_index gets set to the end of the navigation
// stack if the current navigation is invalid.
TEST_F(LocalSessionEventHandlerImplTest,
       SetSessionTabFromDelegateCurrentInvalid) {
  TestSyncedTabDelegate* tab =
      AddTabWithTime(AddWindow()->GetSessionId(), kFoo1, kTime0);
  tab->Navigate(std::string(""), kTime1);
  tab->Navigate(kBar1, kTime2);
  tab->Navigate(kBar2, kTime3);
  tab->set_current_entry_index(1);

  InitHandler();

  sessions::SessionTab session_tab;
  handler_->SetSessionTabFromDelegateForTest(*tab, kTime6, &session_tab);

  EXPECT_EQ(2, session_tab.current_navigation_index);
  ASSERT_EQ(3u, session_tab.navigations.size());
}

// Tests that for supervised users blocked navigations are recorded and marked
// as such, while regular navigations are marked as allowed.
TEST_F(LocalSessionEventHandlerImplTest, BlockedNavigations) {
  TestSyncedTabDelegate* tab =
      AddTabWithTime(AddWindow()->GetSessionId(), kFoo1, kTime1);

  auto entry2 = std::make_unique<sessions::SerializedNavigationEntry>();
  GURL url2("http://blocked.com/foo");
  SerializedNavigationEntryTestHelper::SetVirtualURL(GURL(url2), entry2.get());
  SerializedNavigationEntryTestHelper::SetTimestamp(kTime2, entry2.get());

  auto entry3 = std::make_unique<sessions::SerializedNavigationEntry>();
  GURL url3("http://evil.com");
  SerializedNavigationEntryTestHelper::SetVirtualURL(GURL(url3), entry3.get());
  SerializedNavigationEntryTestHelper::SetTimestamp(kTime3, entry3.get());

  std::vector<std::unique_ptr<sessions::SerializedNavigationEntry>>
      blocked_navigations;
  blocked_navigations.push_back(std::move(entry2));
  blocked_navigations.push_back(std::move(entry3));

  tab->set_is_supervised(true);
  tab->set_blocked_navigations(blocked_navigations);

  sessions::SessionTab session_tab;
  session_tab.window_id.set_id(1);
  session_tab.tab_id.set_id(1);
  session_tab.tab_visual_index = 1;
  session_tab.current_navigation_index = 1;
  session_tab.pinned = true;
  session_tab.extension_app_id = "app id";
  session_tab.user_agent_override = "override";
  session_tab.timestamp = kTime5;
  session_tab.navigations.push_back(
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://www.example.com", "Example"));
  session_tab.session_storage_persistent_id = "persistent id";

  InitHandler();
  handler_->SetSessionTabFromDelegateForTest(*tab, kTime4, &session_tab);

  EXPECT_EQ(tab->GetWindowId(), session_tab.window_id);
  EXPECT_EQ(tab->GetSessionId(), session_tab.tab_id);
  EXPECT_EQ(0, session_tab.tab_visual_index);
  EXPECT_EQ(0, session_tab.current_navigation_index);
  EXPECT_FALSE(session_tab.pinned);
  EXPECT_TRUE(session_tab.extension_app_id.empty());
  EXPECT_TRUE(session_tab.user_agent_override.empty());
  EXPECT_EQ(kTime4, session_tab.timestamp);
  ASSERT_EQ(3u, session_tab.navigations.size());
  EXPECT_EQ(GURL(kFoo1), session_tab.navigations[0].virtual_url());
  EXPECT_EQ(url2, session_tab.navigations[1].virtual_url());
  EXPECT_EQ(url3, session_tab.navigations[2].virtual_url());
  EXPECT_EQ(kTime1, session_tab.navigations[0].timestamp());
  EXPECT_EQ(kTime2, session_tab.navigations[1].timestamp());
  EXPECT_EQ(kTime3, session_tab.navigations[2].timestamp());
  EXPECT_EQ(SerializedNavigationEntry::STATE_ALLOWED,
            session_tab.navigations[0].blocked_state());
  EXPECT_EQ(SerializedNavigationEntry::STATE_BLOCKED,
            session_tab.navigations[1].blocked_state());
  EXPECT_EQ(SerializedNavigationEntry::STATE_BLOCKED,
            session_tab.navigations[2].blocked_state());
  EXPECT_TRUE(session_tab.session_storage_persistent_id.empty());
}

// Tests that calling AssociateWindowsAndTabs() handles well the case with no
// open tabs or windows.
TEST_F(LocalSessionEventHandlerImplTest, AssociateWindowsAndTabsIfEmpty) {
  EXPECT_CALL(mock_delegate_, CreateLocalSessionWriteBatch()).Times(0);
  EXPECT_CALL(mock_delegate_, OnPageFaviconUpdated(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnFaviconVisited(_, _)).Times(0);

  StrictMock<MockWriteBatch> mock_batch;
  EXPECT_CALL(mock_batch, DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/0,
                                                 /*num_tabs=*/0)));

  InitHandler(&mock_batch);
}

// Tests that calling AssociateWindowsAndTabs() reflects the open tabs in a) the
// SyncSessionTracker and b) the delegate.
TEST_F(LocalSessionEventHandlerImplTest, AssociateWindowsAndTabs) {
  AddWindow(kWindowId1);
  AddTab(kWindowId1, kFoo1, kTabId1);
  AddWindow(kWindowId2);
  AddTab(kWindowId2, kBar1, kTabId2);
  AddTab(kWindowId2, kBar2, kTabId3)->Navigate(kBaz1);

  EXPECT_CALL(mock_delegate_, CreateLocalSessionWriteBatch()).Times(0);
  EXPECT_CALL(mock_delegate_, OnPageFaviconUpdated(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnFaviconVisited(GURL(kBar2), _)).Times(0);

  EXPECT_CALL(mock_delegate_, OnFaviconVisited(GURL(kFoo1), _));
  EXPECT_CALL(mock_delegate_, OnFaviconVisited(GURL(kBar1), _));
  EXPECT_CALL(mock_delegate_, OnFaviconVisited(GURL(kBaz1), _));

  StrictMock<MockWriteBatch> mock_batch;
  EXPECT_CALL(mock_batch, DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/2,
                                                 /*num_tabs=*/3)));
  EXPECT_CALL(mock_batch, DoAdd(MatchesTab(kSessionTag, kWindowId1, kTabId1,
                                           std::vector<std::string>{kFoo1})));
  EXPECT_CALL(mock_batch, DoAdd(MatchesTab(kSessionTag, kWindowId2, kTabId2,
                                           std::vector<std::string>{kBar1})));
  EXPECT_CALL(mock_batch,
              DoAdd(MatchesTab(kSessionTag, kWindowId2, kTabId3,
                               std::vector<std::string>{kBar2, kBaz1})));

  InitHandler(&mock_batch);
}

TEST_F(LocalSessionEventHandlerImplTest, PropagateNewNavigation) {
  AddWindow(kWindowId1);
  TestSyncedTabDelegate* tab = AddTab(kWindowId1, kFoo1, kTabId1);

  InitHandler();

  auto update_mock_batch = std::make_unique<StrictMock<MockWriteBatch>>();
  // Note that the header is reported again, although it hasn't changed. This is
  // OK because sync will avoid updating an entity with identical content.
  EXPECT_CALL(*update_mock_batch,
              DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/1,
                                     /*num_tabs=*/1)));
  EXPECT_CALL(*update_mock_batch,
              DoUpdate(MatchesTab(kSessionTag, kWindowId1, kTabId1,
                                  std::vector<std::string>{kFoo1, kBar1})));
  EXPECT_CALL(*update_mock_batch, Commit());

  EXPECT_CALL(mock_delegate_, CreateLocalSessionWriteBatch())
      .WillOnce(Return(ByMove(std::move(update_mock_batch))));

  tab->Navigate(kBar1);
}

TEST_F(LocalSessionEventHandlerImplTest, PropagateNewTab) {
  AddWindow(kWindowId1);
  AddTab(kWindowId1, kFoo1, kTabId1);

  InitHandler();

  // Tab creation triggers an update event due to the tab parented notification,
  // so the event handler issues two commits as well (one for tab creation, one
  // for tab update). During the first update, however, the tab is not syncable
  // and is hence skipped.
  auto tab_create_mock_batch = std::make_unique<StrictMock<MockWriteBatch>>();
  EXPECT_CALL(*tab_create_mock_batch,
              DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/1,
                                     /*num_tabs=*/1)));
  EXPECT_CALL(*tab_create_mock_batch, Commit());

  auto navigation_mock_batch = std::make_unique<StrictMock<MockWriteBatch>>();
  EXPECT_CALL(*navigation_mock_batch,
              DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/1,
                                     /*num_tabs=*/2)));
  EXPECT_CALL(*navigation_mock_batch,
              DoAdd(MatchesTab(kSessionTag, kWindowId1, kTabId2,
                               std::vector<std::string>{kBar1})));
  EXPECT_CALL(*navigation_mock_batch, Commit());

  EXPECT_CALL(mock_delegate_, CreateLocalSessionWriteBatch())
      .WillOnce(Return(ByMove(std::move(tab_create_mock_batch))))
      .WillOnce(Return(ByMove(std::move(navigation_mock_batch))));

  AddTab(kWindowId1, kBar1, kTabId2);
}

TEST_F(LocalSessionEventHandlerImplTest, PropagateNewWindow) {
  AddWindow(kWindowId1);
  AddTab(kWindowId1, kFoo1, kTabId1);
  AddTab(kWindowId1, kBar1, kTabId2);

  InitHandler();

  // Window creation triggers an update event due to the tab parented
  // notification, so the event handler issues two commits as well (one for
  // window creation, one for tab update). During the first update, however,
  // the window is not syncable and is hence skipped.
  auto tab_create_mock_batch = std::make_unique<StrictMock<MockWriteBatch>>();
  EXPECT_CALL(*tab_create_mock_batch,
              DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/1,
                                     /*num_tabs=*/2)));
  EXPECT_CALL(*tab_create_mock_batch, Commit());

  auto navigation_mock_batch = std::make_unique<StrictMock<MockWriteBatch>>();
  EXPECT_CALL(*navigation_mock_batch,
              DoUpdate(MatchesHeader(kSessionTag, /*num_windows=*/2,
                                     /*num_tabs=*/3)));
  EXPECT_CALL(*navigation_mock_batch,
              DoAdd(MatchesTab(kSessionTag, kWindowId2, kTabId3,
                               std::vector<std::string>{kBaz1})));
  EXPECT_CALL(*navigation_mock_batch, Commit());

  EXPECT_CALL(mock_delegate_, CreateLocalSessionWriteBatch())
      .WillOnce(Return(ByMove(std::move(tab_create_mock_batch))))
      .WillOnce(Return(ByMove(std::move(navigation_mock_batch))));

  AddWindow(kWindowId2);
  AddTab(kWindowId2, kBaz1, kTabId3);
}

}  // namespace
}  // namespace sync_sessions
