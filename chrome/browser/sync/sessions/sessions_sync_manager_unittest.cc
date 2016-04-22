// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/glue/session_sync_test_helper.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_driver/device_info.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;
using sync_driver::DeviceInfo;
using sync_driver::LocalDeviceInfoProvider;
using sync_driver::LocalDeviceInfoProviderMock;
using sync_driver::SyncedSession;
using syncer::SyncChange;
using syncer::SyncData;

namespace browser_sync {

namespace {

class SessionNotificationObserver {
 public:
  SessionNotificationObserver()
      : notified_of_update_(false), notified_of_refresh_(false) {}
  void NotifyOfUpdate() { notified_of_update_ = true; }
  void NotifyOfRefresh() { notified_of_refresh_ = true; }

  bool notified_of_update() const { return notified_of_update_; }
  bool notified_of_refresh() const { return notified_of_refresh_; }

  void Reset() {
    notified_of_update_ = false;
    notified_of_refresh_ = false;
  }

 private:
  bool notified_of_update_;
  bool notified_of_refresh_;
};

class SyncedWindowDelegateOverride : public SyncedWindowDelegate {
 public:
  explicit SyncedWindowDelegateOverride(const SyncedWindowDelegate* wrapped)
      : wrapped_(wrapped) {
  }
  ~SyncedWindowDelegateOverride() override {}

  bool HasWindow() const override { return wrapped_->HasWindow(); }

  SessionID::id_type GetSessionId() const override {
    return wrapped_->GetSessionId();
  }

  int GetTabCount() const override { return wrapped_->GetTabCount(); }

  int GetActiveIndex() const override { return wrapped_->GetActiveIndex(); }

  bool IsApp() const override { return wrapped_->IsApp(); }

  bool IsTypeTabbed() const override { return wrapped_->IsTypeTabbed(); }

  bool IsTypePopup() const override { return wrapped_->IsTypePopup(); }

  bool IsTabPinned(const SyncedTabDelegate* tab) const override {
    return wrapped_->IsTabPinned(tab);
  }

  SyncedTabDelegate* GetTabAt(int index) const override {
    if (tab_overrides_.find(index) != tab_overrides_.end())
      return tab_overrides_.find(index)->second;

    return wrapped_->GetTabAt(index);
  }

  void OverrideTabAt(int index,
                     SyncedTabDelegate* delegate,
                     SessionID::id_type tab_id) {
    tab_overrides_[index] = delegate;
    tab_id_overrides_[index] = tab_id;
  }

  SessionID::id_type GetTabIdAt(int index) const override {
    if (tab_id_overrides_.find(index) != tab_id_overrides_.end())
      return tab_id_overrides_.find(index)->second;
    return wrapped_->GetTabIdAt(index);
  }

  bool IsSessionRestoreInProgress() const override {
    return wrapped_->IsSessionRestoreInProgress();
  }

  bool ShouldSync() const override { return wrapped_->ShouldSync(); }

 private:
  std::map<int, SyncedTabDelegate*> tab_overrides_;
  std::map<int, SessionID::id_type> tab_id_overrides_;
  const SyncedWindowDelegate* const wrapped_;
};

class TestSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter(
      const std::set<const SyncedWindowDelegate*>& delegates)
      : delegates_(delegates) {}

  std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() override {
    return delegates_;
  }

  const SyncedWindowDelegate* FindById(SessionID::id_type id) override {
    for (auto* window : delegates_) {
      if (window->GetSessionId() == id)
        return window;
    }
    return nullptr;
  }

 private:
  const std::set<const SyncedWindowDelegate*> delegates_;
};

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output) {}
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (error_.IsSet()) {
      syncer::SyncError error = error_;
      error_ = syncer::SyncError();
      return error;
    }

    if (output_)
      output_->insert(output_->end(), change_list.begin(), change_list.end());

    return syncer::SyncError();
  }

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return sync_data_to_return_;
  }

  void FailProcessSyncChangesWith(const syncer::SyncError& error) {
    error_ = error;
  }

  void SetSyncDataToReturn(const syncer::SyncDataList& data) {
    sync_data_to_return_ = data;
  }

 private:
  syncer::SyncError error_;
  syncer::SyncChangeList* output_;
  syncer::SyncDataList sync_data_to_return_;
};

void ExpectAllOfChangesType(const syncer::SyncChangeList& changes,
                            const syncer::SyncChange::SyncChangeType type) {
  for (const syncer::SyncChange& change : changes) {
    EXPECT_EQ(type, change.change_type());
  }
}

int CountIfTagMatches(const syncer::SyncChangeList& changes,
                      const std::string& tag) {
  return std::count_if(
      changes.begin(), changes.end(), [&tag](const syncer::SyncChange& change) {
        return change.sync_data().GetSpecifics().session().session_tag() == tag;
      });
}

int CountIfTagMatches(
    const std::vector<const sync_driver::SyncedSession*>& sessions,
    const std::string& tag) {
  return std::count_if(sessions.begin(), sessions.end(),
                       [&tag](const sync_driver::SyncedSession* session) {
                         return session->session_tag == tag;
                       });
}

// Creates a field trial with the specified |trial_name| and |group_name| and
// registers an associated |variation_id| for it for the given |service|.
void CreateAndActivateFieldTrial(const std::string& trial_name,
                                 const std::string& group_name,
                                 variations::VariationID variation_id,
                                 variations::IDCollectionKey service) {
  base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  variations::AssociateGoogleVariationID(service, trial_name, group_name,
                                         variation_id);
  // Access the trial to activate it.
  base::FieldTrialList::FindFullName(trial_name);
}

class DummyRouter : public LocalSessionEventRouter {
 public:
  ~DummyRouter() override {}
  void StartRoutingTo(LocalSessionEventHandler* handler) override {}
  void Stop() override {}
};

std::unique_ptr<LocalSessionEventRouter> NewDummyRouter() {
  return std::unique_ptr<LocalSessionEventRouter>(new DummyRouter());
}

// Provides ability to override SyncedWindowDelegatesGetter.
// All other calls are passed through to the original SyncSessionsClient.
class SyncSessionsClientShim : public sync_sessions::SyncSessionsClient {
 public:
  SyncSessionsClientShim(
      sync_sessions::SyncSessionsClient* sync_sessions_client)
      : sync_sessions_client_(sync_sessions_client),
        synced_window_getter_(nullptr) {}
  ~SyncSessionsClientShim() override {}

  bookmarks::BookmarkModel* GetBookmarkModel() override {
    return sync_sessions_client_->GetBookmarkModel();
  }

  favicon::FaviconService* GetFaviconService() override {
    return sync_sessions_client_->GetFaviconService();
  }

  history::HistoryService* GetHistoryService() override {
    return sync_sessions_client_->GetHistoryService();
  }

  bool ShouldSyncURL(const GURL& url) const override {
    return sync_sessions_client_->ShouldSyncURL(url);
  }

  browser_sync::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    // The idea here is to allow the test code override the default
    // SyncedWindowDelegatesGetter provided by |sync_sessions_client_|.
    // If |synced_window_getter_| is explicitly set, return it; otherwise return
    // the default one provided by |sync_sessions_client_|.
    return synced_window_getter_
               ? synced_window_getter_
               : sync_sessions_client_->GetSyncedWindowDelegatesGetter();
  }

  std::unique_ptr<browser_sync::LocalSessionEventRouter>
  GetLocalSessionEventRouter() override {
    return sync_sessions_client_->GetLocalSessionEventRouter();
  }

  void set_synced_window_getter(
      browser_sync::SyncedWindowDelegatesGetter* synced_window_getter) {
    synced_window_getter_ = synced_window_getter;
  }

 private:
  sync_sessions::SyncSessionsClient* const sync_sessions_client_;
  browser_sync::SyncedWindowDelegatesGetter* synced_window_getter_;
};

}  // namespace

class SessionsSyncManagerTest
    : public BrowserWithTestWindowTest {
 protected:
  SessionsSyncManagerTest()
      : test_processor_(NULL) {
    local_device_.reset(new LocalDeviceInfoProviderMock(
        "cache_guid",
        "Wayne Gretzky's Hacking Box",
        "Chromium 10k",
        "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        "device_id"));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    sync_client_.reset(new browser_sync::ChromeSyncClient(profile()));
    sessions_client_shim_.reset(
        new SyncSessionsClientShim(sync_client_->GetSyncSessionsClient()));
    browser_sync::NotificationServiceSessionsRouter* router(
        new browser_sync::NotificationServiceSessionsRouter(
            profile(), GetSyncSessionsClient(),
            syncer::SyncableService::StartSyncFlare()));
    sync_prefs_.reset(new sync_driver::SyncPrefs(profile()->GetPrefs()));
    manager_.reset(new SessionsSyncManager(
        GetSyncSessionsClient(), sync_prefs_.get(), local_device_.get(),
        std::unique_ptr<LocalSessionEventRouter>(router),
        base::Bind(&SessionNotificationObserver::NotifyOfUpdate,
                   base::Unretained(&observer_)),
        base::Bind(&SessionNotificationObserver::NotifyOfRefresh,
                   base::Unretained(&observer_))));
  }

  void TearDown() override {
    test_processor_ = NULL;
    helper()->Reset();
    sync_prefs_.reset();
    manager_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  const DeviceInfo* GetLocalDeviceInfo() {
    return local_device_->GetLocalDeviceInfo();
  }

  SessionsSyncManager* manager() { return manager_.get(); }
  SessionSyncTestHelper* helper() { return &helper_; }
  LocalDeviceInfoProvider* local_device() { return local_device_.get(); }
  SessionNotificationObserver* observer() { return &observer_; }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    test_processor_ = new TestSyncProcessorStub(output);
    syncer::SyncMergeResult result = manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, initial_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(test_processor_),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), NULL);
  }

  void TriggerProcessSyncChangesError() {
    test_processor_->FailProcessSyncChangesWith(syncer::SyncError(
        FROM_HERE, syncer::SyncError::DATATYPE_ERROR, "Error",
        syncer::SESSIONS));
  }

  void SetSyncData(const syncer::SyncDataList& data) {
     test_processor_->SetSyncDataToReturn(data);
  }

  syncer::SyncChangeList* FilterOutLocalHeaderChanges(
      syncer::SyncChangeList* list) {
    syncer::SyncChangeList::iterator it = list->begin();
    bool found = false;
    while (it != list->end()) {
      if (it->sync_data().IsLocal() &&
          syncer::SyncDataLocal(it->sync_data()).GetTag() ==
              manager_->current_machine_tag()) {
        EXPECT_TRUE(SyncChange::ACTION_ADD == it->change_type() ||
                    SyncChange::ACTION_UPDATE == it->change_type());
        it = list->erase(it);
        found = true;
      } else {
        ++it;
      }
    }
    EXPECT_TRUE(found);
    return list;
  }

  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() {
    return sessions_client_shim_.get();
  }

  sync_driver::SyncPrefs* sync_prefs() { return sync_prefs_.get(); }

  void set_synced_window_getter(
      browser_sync::SyncedWindowDelegatesGetter* synced_window_getter) {
    sessions_client_shim_->set_synced_window_getter(synced_window_getter);
  }

  syncer::SyncChange MakeRemoteChange(
      int64_t id,
      const sync_pb::SessionSpecifics& specifics,
      SyncChange::SyncChangeType type) const {
    return syncer::SyncChange(FROM_HERE, type, CreateRemoteData(specifics, id));
  }

  void AddTabsToChangeList(const std::vector<sync_pb::SessionSpecifics>& batch,
                           SyncChange::SyncChangeType type,
                           syncer::SyncChangeList* change_list) const {
    for (const auto& specifics : batch) {
      change_list->push_back(syncer::SyncChange(
          FROM_HERE, type,
          CreateRemoteData(specifics, specifics.tab_node_id())));
    }
  }

  void AddToSyncDataList(const sync_pb::SessionSpecifics& specifics,
                         syncer::SyncDataList* list,
                         base::Time mtime) const {
    list->push_back(CreateRemoteData(specifics, 1, mtime));
  }

  void AddTabsToSyncDataList(const std::vector<sync_pb::SessionSpecifics>& tabs,
                             syncer::SyncDataList* list) const {
    for (size_t i = 0; i < tabs.size(); i++) {
      AddToSyncDataList(tabs[i], list, base::Time::FromInternalValue(i + 1));
    }
  }

  syncer::SyncData CreateRemoteData(const sync_pb::SessionSpecifics& specifics,
                                    int64_t id = 1,
                                    base::Time mtime = base::Time()) const {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(specifics);
    return CreateRemoteData(entity, id, mtime);
  }

  syncer::SyncData CreateRemoteData(const sync_pb::EntitySpecifics& entity,
                                    int64_t id,
                                    base::Time mtime) const {
    return SyncData::CreateRemoteData(
        id, entity, mtime, syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create(),
        SessionsSyncManager::TagHashFromSpecifics(entity.session()));
  }

 private:
  std::unique_ptr<browser_sync::ChromeSyncClient> sync_client_;
  std::unique_ptr<SyncSessionsClientShim> sessions_client_shim_;
  std::unique_ptr<sync_driver::SyncPrefs> sync_prefs_;
  SessionNotificationObserver observer_;
  std::unique_ptr<SessionsSyncManager> manager_;
  SessionSyncTestHelper helper_;
  TestSyncProcessorStub* test_processor_;
  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;
};

// Test that the SyncSessionManager can properly fill in a SessionHeader.
TEST_F(SessionsSyncManagerTest, PopulateSessionHeader) {
  sync_pb::SessionHeader header_s;
  header_s.set_client_name("Client 1");
  header_s.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);

  SyncedSession session;
  base::Time time = base::Time::Now();
  SessionsSyncManager::PopulateSessionHeaderFromSpecifics(
      header_s, time, &session);
  ASSERT_EQ("Client 1", session.session_name);
  ASSERT_EQ(SyncedSession::TYPE_WIN, session.device_type);
  ASSERT_EQ(time, session.modified_time);
}

// Test translation between protobuf types and chrome session types.
TEST_F(SessionsSyncManagerTest, PopulateSessionWindow) {
  sync_pb::SessionWindow window_s;
  window_s.add_tab(0);
  window_s.set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  window_s.set_selected_tab_index(1);

  std::string tag = "tag";
  SyncedSession* session = manager()->session_tracker_.GetSession(tag);
  manager()->session_tracker_.PutWindowInSession(tag, 0);
  manager()->BuildSyncedSessionFromSpecifics(
      tag, window_s, base::Time(), session->windows[0]);
  ASSERT_EQ(1U, session->windows[0]->tabs.size());
  ASSERT_EQ(1, session->windows[0]->selected_tab_index);
  ASSERT_EQ(sessions::SessionWindow::TYPE_TABBED, session->windows[0]->type);
  ASSERT_EQ(1U, manager()->session_tracker_.num_synced_sessions());
  ASSERT_EQ(1U,
            manager()->session_tracker_.num_synced_tabs(std::string("tag")));
}

namespace {

class SyncedTabDelegateFake : public SyncedTabDelegate {
 public:
  SyncedTabDelegateFake()
      : current_entry_index_(0), is_supervised_(false), sync_id_(-1) {}
  ~SyncedTabDelegateFake() override {}

  bool IsInitialBlankNavigation() const override {
    // This differs from NavigationControllerImpl, which has an initial blank
    // NavigationEntry.
    return GetEntryCount() == 0;
  }
  int GetCurrentEntryIndex() const override { return current_entry_index_; }
  void set_current_entry_index(int i) {
    current_entry_index_ = i;
  }

  void AppendEntry(std::unique_ptr<content::NavigationEntry> entry) {
    entries_.push_back(std::move(entry));
  }

  GURL GetVirtualURLAtIndex(int i) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return GURL();
    return entries_[i]->GetVirtualURL();
  }

  GURL GetFaviconURLAtIndex(int i) const override { return GURL(); }

  ui::PageTransition GetTransitionAtIndex(int i) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return ui::PAGE_TRANSITION_LINK;
    return entries_[i]->GetTransitionType();
  }

  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return;
    *serialized_entry =
        sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
            i, *entries_[i]);
  }

  int GetEntryCount() const override { return entries_.size(); }

  SessionID::id_type GetWindowId() const override {
    return SessionID::id_type();
  }

  SessionID::id_type GetSessionId() const override {
    return SessionID::id_type();
  }

  bool IsBeingDestroyed() const override { return false; }
  std::string GetExtensionAppId() const override { return std::string(); }
  bool ProfileIsSupervised() const override { return is_supervised_; }
  void set_is_supervised(bool is_supervised) { is_supervised_ = is_supervised; }
  const std::vector<const sessions::SerializedNavigationEntry*>*
  GetBlockedNavigations() const override {
    return &blocked_navigations_.get();
  }
  void set_blocked_navigations(
      std::vector<const content::NavigationEntry*>* navs) {
    for (auto* entry : *navs) {
      std::unique_ptr<sessions::SerializedNavigationEntry> serialized_entry(
          new sessions::SerializedNavigationEntry());
      *serialized_entry =
          sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
              blocked_navigations_.size(), *entry);
      blocked_navigations_.push_back(serialized_entry.release());
    }
  }
  bool IsPlaceholderTab() const override { return true; }

  // Session sync related methods.
  int GetSyncId() const override { return sync_id_; }
  void SetSyncId(int sync_id) override { sync_id_ = sync_id; }

  bool ShouldSync(sync_sessions::SyncSessionsClient* sessions_client) override {
    return false;
  }

  void reset() {
    current_entry_index_ = 0;
    sync_id_ = -1;
    entries_.clear();
  }

 private:
  int current_entry_index_;
  bool is_supervised_;
  int sync_id_;
  ScopedVector<const sessions::SerializedNavigationEntry> blocked_navigations_;
  ScopedVector<content::NavigationEntry> entries_;
};

}  // namespace

static const base::Time kTime0 = base::Time::FromInternalValue(100);
static const base::Time kTime1 = base::Time::FromInternalValue(110);
static const base::Time kTime2 = base::Time::FromInternalValue(120);
static const base::Time kTime3 = base::Time::FromInternalValue(130);
static const base::Time kTime4 = base::Time::FromInternalValue(140);
static const base::Time kTime5 = base::Time::FromInternalValue(150);
static const base::Time kTime6 = base::Time::FromInternalValue(160);
static const base::Time kTime7 = base::Time::FromInternalValue(170);
static const base::Time kTime8 = base::Time::FromInternalValue(180);
static const base::Time kTime9 = base::Time::FromInternalValue(190);

// Populate the mock tab delegate with some data and navigation
// entries and make sure that setting a SessionTab from it preserves
// those entries (and clobbers any existing data).
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegate) {
  // Create a tab with three valid entries.
  SyncedTabDelegateFake tab;
  std::unique_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  GURL url1("http://www.google.com/");
  entry1->SetVirtualURL(url1);
  entry1->SetTimestamp(kTime1);
  entry1->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  GURL url2("http://www.noodle.com/");
  entry2->SetVirtualURL(url2);
  entry2->SetTimestamp(kTime2);
  entry2->SetHttpStatusCode(201);
  std::unique_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  GURL url3("http://www.doodle.com/");
  entry3->SetVirtualURL(url3);
  entry3->SetTimestamp(kTime3);
  entry3->SetHttpStatusCode(202);

  tab.AppendEntry(std::move(entry1));
  tab.AppendEntry(std::move(entry2));
  tab.AppendEntry(std::move(entry3));
  tab.set_current_entry_index(2);

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
  manager()->SetSessionTabFromDelegate(tab, kTime4, &session_tab);

  EXPECT_EQ(0, session_tab.window_id.id());
  EXPECT_EQ(0, session_tab.tab_id.id());
  EXPECT_EQ(0, session_tab.tab_visual_index);
  EXPECT_EQ(2, session_tab.current_navigation_index);
  EXPECT_FALSE(session_tab.pinned);
  EXPECT_TRUE(session_tab.extension_app_id.empty());
  EXPECT_TRUE(session_tab.user_agent_override.empty());
  EXPECT_EQ(kTime4, session_tab.timestamp);
  ASSERT_EQ(3u, session_tab.navigations.size());
  EXPECT_EQ(url1, session_tab.navigations[0].virtual_url());
  EXPECT_EQ(url2, session_tab.navigations[1].virtual_url());
  EXPECT_EQ(url3, session_tab.navigations[2].virtual_url());
  EXPECT_EQ(kTime1, session_tab.navigations[0].timestamp());
  EXPECT_EQ(kTime2, session_tab.navigations[1].timestamp());
  EXPECT_EQ(kTime3, session_tab.navigations[2].timestamp());
  EXPECT_EQ(200, session_tab.navigations[0].http_status_code());
  EXPECT_EQ(201, session_tab.navigations[1].http_status_code());
  EXPECT_EQ(202, session_tab.navigations[2].http_status_code());
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
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegateNavigationIndex) {
  SyncedTabDelegateFake tab;
  std::unique_ptr<content::NavigationEntry> entry0(
      content::NavigationEntry::Create());
  GURL url0("http://www.google.com/");
  entry0->SetVirtualURL(url0);
  entry0->SetTimestamp(kTime0);
  entry0->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  GURL url1("http://www.zoogle.com/");
  entry1->SetVirtualURL(url1);
  entry1->SetTimestamp(kTime1);
  entry1->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  GURL url2("http://www.noogle.com/");
  entry2->SetVirtualURL(url2);
  entry2->SetTimestamp(kTime2);
  entry2->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  GURL url3("http://www.doogle.com/");
  entry3->SetVirtualURL(url3);
  entry3->SetTimestamp(kTime3);
  entry3->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry4(
      content::NavigationEntry::Create());
  GURL url4("http://www.yoogle.com/");
  entry4->SetVirtualURL(url4);
  entry4->SetTimestamp(kTime4);
  entry4->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry5(
      content::NavigationEntry::Create());
  GURL url5("http://www.foogle.com/");
  entry5->SetVirtualURL(url5);
  entry5->SetTimestamp(kTime5);
  entry5->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry6(
      content::NavigationEntry::Create());
  GURL url6("http://www.boogle.com/");
  entry6->SetVirtualURL(url6);
  entry6->SetTimestamp(kTime6);
  entry6->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry7(
      content::NavigationEntry::Create());
  GURL url7("http://www.moogle.com/");
  entry7->SetVirtualURL(url7);
  entry7->SetTimestamp(kTime7);
  entry7->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry8(
      content::NavigationEntry::Create());
  GURL url8("http://www.poogle.com/");
  entry8->SetVirtualURL(url8);
  entry8->SetTimestamp(kTime8);
  entry8->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry9(
      content::NavigationEntry::Create());
  GURL url9("http://www.roogle.com/");
  entry9->SetVirtualURL(url9);
  entry9->SetTimestamp(kTime9);
  entry9->SetHttpStatusCode(200);

  tab.AppendEntry(std::move(entry0));
  tab.AppendEntry(std::move(entry1));
  tab.AppendEntry(std::move(entry2));
  tab.AppendEntry(std::move(entry3));
  tab.AppendEntry(std::move(entry4));
  tab.AppendEntry(std::move(entry5));
  tab.AppendEntry(std::move(entry6));
  tab.AppendEntry(std::move(entry7));
  tab.AppendEntry(std::move(entry8));
  tab.AppendEntry(std::move(entry9));
  tab.set_current_entry_index(8);

  sessions::SessionTab session_tab;
  manager()->SetSessionTabFromDelegate(tab, kTime9, &session_tab);

  EXPECT_EQ(6, session_tab.current_navigation_index);
  ASSERT_EQ(8u, session_tab.navigations.size());
  EXPECT_EQ(url2, session_tab.navigations[0].virtual_url());
  EXPECT_EQ(url3, session_tab.navigations[1].virtual_url());
  EXPECT_EQ(url4, session_tab.navigations[2].virtual_url());
}

// Ensure the current_navigation_index gets set to the end of the navigation
// stack if the current navigation is invalid.
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegateCurrentInvalid) {
  SyncedTabDelegateFake tab;
  std::unique_ptr<content::NavigationEntry> entry0(
      content::NavigationEntry::Create());
  entry0->SetVirtualURL(GURL("http://www.google.com"));
  entry0->SetTimestamp(kTime0);
  entry0->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL(""));
  entry1->SetTimestamp(kTime1);
  entry1->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noogle.com"));
  entry2->SetTimestamp(kTime2);
  entry2->SetHttpStatusCode(200);
  std::unique_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doogle.com"));
  entry3->SetTimestamp(kTime3);
  entry3->SetHttpStatusCode(200);

  tab.AppendEntry(std::move(entry0));
  tab.AppendEntry(std::move(entry1));
  tab.AppendEntry(std::move(entry2));
  tab.AppendEntry(std::move(entry3));
  tab.set_current_entry_index(1);

  sessions::SessionTab session_tab;
  manager()->SetSessionTabFromDelegate(tab, kTime9, &session_tab);

  EXPECT_EQ(2, session_tab.current_navigation_index);
  ASSERT_EQ(3u, session_tab.navigations.size());
}

// Tests that variation ids are set correctly.
TEST_F(SessionsSyncManagerTest, SetVariationIds) {
  // Create two trials with a group which has a variation id for Chrome Sync
  // and one with a variation id for another service.
  const variations::VariationID kVariationId1 = 3300200;
  const variations::VariationID kVariationId2 = 3300300;
  const variations::VariationID kVariationId3 = 3300400;

  base::FieldTrialList field_trial_list(NULL);
  CreateAndActivateFieldTrial("trial name 1", "group name", kVariationId1,
                              variations::CHROME_SYNC_SERVICE);
  CreateAndActivateFieldTrial("trial name 2", "group name", kVariationId2,
                              variations::CHROME_SYNC_SERVICE);
  CreateAndActivateFieldTrial("trial name 3", "group name", kVariationId3,
                              variations::GOOGLE_UPDATE_SERVICE);

  sessions::SessionTab session_tab;
  manager()->SetVariationIds(&session_tab);

  ASSERT_EQ(2u, session_tab.variation_ids.size());
  EXPECT_EQ(kVariationId1, session_tab.variation_ids[0]);
  EXPECT_EQ(kVariationId2, session_tab.variation_ids[1]);
}

// Tests that for supervised users blocked navigations are recorded and marked
// as such, while regular navigations are marked as allowed.
TEST_F(SessionsSyncManagerTest, BlockedNavigations) {
  SyncedTabDelegateFake tab;
  std::unique_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  GURL url1("http://www.google.com/");
  entry1->SetVirtualURL(url1);
  entry1->SetTimestamp(kTime1);
  tab.AppendEntry(std::move(entry1));

  std::unique_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  GURL url2("http://blocked.com/foo");
  entry2->SetVirtualURL(url2);
  entry2->SetTimestamp(kTime2);
  std::unique_ptr<content::NavigationEntry> entry3(
      content::NavigationEntry::Create());
  GURL url3("http://evil.com/");
  entry3->SetVirtualURL(url3);
  entry3->SetTimestamp(kTime3);
  ScopedVector<const content::NavigationEntry> blocked_navigations;
  blocked_navigations.push_back(std::move(entry2));
  blocked_navigations.push_back(std::move(entry3));

  tab.set_is_supervised(true);
  tab.set_blocked_navigations(&blocked_navigations.get());

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
  manager()->SetSessionTabFromDelegate(tab, kTime4, &session_tab);

  EXPECT_EQ(0, session_tab.window_id.id());
  EXPECT_EQ(0, session_tab.tab_id.id());
  EXPECT_EQ(0, session_tab.tab_visual_index);
  EXPECT_EQ(0, session_tab.current_navigation_index);
  EXPECT_FALSE(session_tab.pinned);
  EXPECT_TRUE(session_tab.extension_app_id.empty());
  EXPECT_TRUE(session_tab.user_agent_override.empty());
  EXPECT_EQ(kTime4, session_tab.timestamp);
  ASSERT_EQ(3u, session_tab.navigations.size());
  EXPECT_EQ(url1, session_tab.navigations[0].virtual_url());
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

// Tests that the local session header objects is created properly in
// presence of no other session activity, once and only once.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionNoTabs) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  EXPECT_FALSE(manager()->current_machine_tag().empty());

  EXPECT_EQ(2U, out.size());
  EXPECT_TRUE(out[0].IsValid());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[0].change_type());
  const SyncData data(out[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data).GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_TRUE(header_s.has_device_type());
  EXPECT_EQ(GetLocalDeviceInfo()->client_name(), header_s.client_name());
  EXPECT_EQ(0, header_s.window_size());

  EXPECT_TRUE(out[1].IsValid());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[1].change_type());
  const SyncData data_2(out[1].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data_2).GetTag());
  const sync_pb::SessionSpecifics& specifics2(data_2.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics2.session_tag());
  EXPECT_TRUE(specifics2.has_header());
  const sync_pb::SessionHeader& header_s2 = specifics2.header();
  EXPECT_EQ(0, header_s2.window_size());

  // Now take that header node and feed it in as input.
  SyncData d(SyncData::CreateRemoteData(
      1,
      data.GetSpecifics(),
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  syncer::SyncDataList in(&d, &d + 1);
  out.clear();
  SessionsSyncManager manager2(GetSyncSessionsClient(), sync_prefs(),
                               local_device(), NewDummyRouter(),
                               base::Closure(), base::Closure());
  syncer::SyncMergeResult result = manager2.MergeDataAndStartSyncing(
      syncer::SESSIONS, in, std::unique_ptr<syncer::SyncChangeProcessor>(
                                new TestSyncProcessorStub(&out)),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));
  ASSERT_FALSE(result.error().IsSet());

  EXPECT_EQ(1U, out.size());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[0].change_type());
  EXPECT_TRUE(out[0].sync_data().GetSpecifics().session().has_header());
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, SwappedOutOnRestore) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  AddTab(browser(), GURL("http://baz1"));
  NavigateAndCommitActiveTab(GURL("http://baz2"));
  const int kRestoredTabId = 1337;
  const int kNewTabId = 2468;

  syncer::SyncDataList in;
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 3 tab add/update pairs, one header update.
  ASSERT_EQ(8U, out.size());

  // For input, we set up:
  // * one "normal" fully loaded tab
  // * one "frozen" tab with no WebContents and a tab_id change
  // * one "frozen" tab with no WebContents and no tab_id change
  SyncData t0(SyncData::CreateRemoteData(
      1,
      out[2].sync_data().GetSpecifics(),
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  sync_pb::EntitySpecifics entity(out[4].sync_data().GetSpecifics());
  entity.mutable_session()->mutable_tab()->set_tab_id(kRestoredTabId);
  SyncData t1(SyncData::CreateRemoteData(
      2,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  SyncData t2(SyncData::CreateRemoteData(
      3,
      out[6].sync_data().GetSpecifics(),
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  in.push_back(t0);
  in.push_back(t1);
  in.push_back(t2);
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);

  const std::set<const SyncedWindowDelegate*>& windows =
      manager()->synced_window_delegates_getter()->GetSyncedWindowDelegates();
  ASSERT_EQ(1U, windows.size());
  SyncedTabDelegateFake t1_override, t2_override;
  t1_override.SetSyncId(1);  // No WebContents by default.
  t2_override.SetSyncId(2);  // No WebContents by default.
  SyncedWindowDelegateOverride window_override(*windows.begin());
  window_override.OverrideTabAt(1, &t1_override, kNewTabId);
  window_override.OverrideTabAt(2, &t2_override,
                                t2.GetSpecifics().session().tab().tab_id());
  std::set<const SyncedWindowDelegate*> delegates;
  delegates.insert(&window_override);
  std::unique_ptr<TestSyncedWindowDelegatesGetter> getter(
      new TestSyncedWindowDelegatesGetter(delegates));
  set_synced_window_getter(getter.get());

  syncer::SyncMergeResult result = manager()->MergeDataAndStartSyncing(
      syncer::SESSIONS, in, std::unique_ptr<syncer::SyncChangeProcessor>(
                                new TestSyncProcessorStub(&out)),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));

  // There should be two changes, one for the fully associated tab, and
  // one for the tab_id update to t1.  t2 shouldn't need to be updated.
  ASSERT_EQ(2U, FilterOutLocalHeaderChanges(&out)->size());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[0].change_type());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[1].change_type());
  EXPECT_EQ(kNewTabId,
            out[1].sync_data().GetSpecifics().session().tab().tab_id());

  // Verify TabLinks.
  SessionsSyncManager::TabLinksMap tab_map = manager()->local_tab_map_;
  ASSERT_EQ(3U, tab_map.size());
  int t2_tab_id = t2.GetSpecifics().session().tab().tab_id();
  EXPECT_EQ(2, tab_map.find(t2_tab_id)->second->tab_node_id());
  EXPECT_EQ(1, tab_map.find(kNewTabId)->second->tab_node_id());
  int t0_tab_id = out[0].sync_data().GetSpecifics().session().tab().tab_id();
  EXPECT_EQ(0, tab_map.find(t0_tab_id)->second->tab_node_id());
  // TODO(tim): Once bug 337057 is fixed, we can issue an OnLocalTabModified
  // from here (using an override similar to above) to return a new tab id
  // and verify that we don't see any node creations in the SyncChangeProcessor
  // (similar to how SessionsSyncManagerTest.OnLocalTabModified works.)
}

// Tests MergeDataAndStartSyncing with sync data but no local data.
TEST_F(SessionsSyncManagerTest, MergeWithInitialForeignSession) {
  std::string tag = "tag1";

  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_list1, &tabs1));
  // Add a second window.
  SessionID::id_type n2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(n2, n2 + arraysize(n2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &initial_data);

  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i],
                                entity.mutable_session());
    initial_data.push_back(CreateRemoteData(entity, i + 10, base::Time()));
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
  EXPECT_TRUE(FilterOutLocalHeaderChanges(&output)->empty());

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  session_reference.push_back(tab_list2);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// This is a combination of MergeWithInitialForeignSession and
// MergeLocalSessionExistingTabs. We repeat some checks performed in each of
// those tests to ensure the common mixed scenario works.
TEST_F(SessionsSyncManagerTest, MergeWithLocalAndForeignTabs) {
  // Local.
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  // Foreign.
  std::string tag = "tag1";
  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_list1, &tabs1));
  syncer::SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &foreign_data);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(4U, output.size());

  // Verify the local header.
  EXPECT_TRUE(output[0].IsValid());
  EXPECT_EQ(SyncChange::ACTION_ADD, output[0].change_type());
  const SyncData data(output[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data).GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_TRUE(header_s.has_device_type());
  EXPECT_EQ(GetLocalDeviceInfo()->client_name(), header_s.client_name());
  EXPECT_EQ(0, header_s.window_size());

  // Verify the tab node creations and updates with content.
  for (int i = 1; i < 3; i++) {
    EXPECT_TRUE(output[i].IsValid());
    const SyncData data(output[i].sync_data());
    EXPECT_TRUE(base::StartsWith(syncer::SyncDataLocal(data).GetTag(),
                                 manager()->current_machine_tag(),
                                 base::CompareCase::SENSITIVE));
    const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
    EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  }
  EXPECT_EQ(SyncChange::ACTION_ADD, output[1].change_type());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, output[2].change_type());
  EXPECT_TRUE(output[2].sync_data().GetSpecifics().session().has_tab());

  // Verify the header was updated to reflect window state.
  EXPECT_TRUE(output[3].IsValid());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, output[3].change_type());
  const SyncData data_2(output[3].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data_2).GetTag());
  const sync_pb::SessionSpecifics& specifics2(data_2.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics2.session_tag());
  EXPECT_TRUE(specifics2.has_header());
  const sync_pb::SessionHeader& header_s2 = specifics2.header();
  EXPECT_EQ(1, header_s2.window_size());

  // Verify foreign data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
  // There should be one and only one foreign session. If VerifySyncedSession
  // was successful above this EXPECT call ensures the local session didn't
  // get mistakenly added to foreign tracking (Similar to ExistingTabs test).
  EXPECT_EQ(1U, foreign_sessions.size());
}

// Tests the common scenario.  Merge with both local and foreign session data
// followed by updates flowing from sync and local.
TEST_F(SessionsSyncManagerTest, UpdatesAfterMixedMerge) {
  // Add local and foreign data.
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  std::string tag1 = "tag1";
  syncer::SyncDataList foreign_data1;
  std::vector<std::vector<SessionID::id_type> > meta1_reference;
  sync_pb::SessionSpecifics meta1;

  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  meta1_reference.push_back(tab_list1);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  meta1 = helper()->BuildForeignSession(tag1, tab_list1, &tabs1);
  foreign_data1.push_back(CreateRemoteData(meta1));
  AddTabsToSyncDataList(tabs1, &foreign_data1);

  syncer::SyncChangeList output1;
  InitWithSyncDataTakeOutput(foreign_data1, &output1);
  ASSERT_EQ(4U, output1.size());

  // Add a second window to the foreign session.
  // TODO(tim): Bug 98892. Add local window too when observers are hooked up.
  SessionID::id_type tab_nums2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(
      tab_nums2, tab_nums2 + arraysize(tab_nums2));
  meta1_reference.push_back(tab_list2);
  helper()->AddWindowSpecifics(1, tab_list2, &meta1);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(tab_list2.size());
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    helper()->BuildTabSpecifics(tag1, 0, tab_list2[i], &tabs2[i]);
  }

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs2, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(1)->second->tabs.size());
  helper()->VerifySyncedSession(tag1, meta1_reference, *(foreign_sessions[0]));

  // Add a new foreign session.
  std::string tag2 = "tag2";
  SessionID::id_type n2[] = {107, 115};
  std::vector<SessionID::id_type> tag2_tab_list(n2, n2 + arraysize(n2));
  std::vector<sync_pb::SessionSpecifics> tag2_tabs;
  sync_pb::SessionSpecifics meta2(helper()->BuildForeignSession(
      tag2, tag2_tab_list, &tag2_tabs));
  changes.push_back(MakeRemoteChange(100, meta2, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tag2_tabs, SyncChange::ACTION_ADD, &changes);

  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type> > meta2_reference;
  meta2_reference.push_back(tag2_tab_list);
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[1]->windows.find(0)->second->tabs.size());
  helper()->VerifySyncedSession(tag2, meta2_reference, *(foreign_sessions[1]));
  foreign_sessions.clear();

  // Remove a tab from a window.
  meta1_reference[0].pop_back();
  tab_list1.pop_back();
  sync_pb::SessionWindow* win = meta1.mutable_header()->mutable_window(0);
  win->clear_tab();
  for (std::vector<int>::const_iterator iter = tab_list1.begin();
       iter != tab_list1.end(); ++iter) {
    win->add_tab(*iter);
  }
  syncer::SyncChangeList removal;
  removal.push_back(MakeRemoteChange(1, meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_UPDATE, &removal);
  manager()->ProcessSyncChanges(FROM_HERE, removal);

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(3U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  helper()->VerifySyncedSession(tag1, meta1_reference, *(foreign_sessions[0]));
}

// Tests that this SyncSessionManager knows how to delete foreign sessions
// if it wants to.
TEST_F(SessionsSyncManagerTest, DeleteForeignSession) {
  InitWithNoSyncData();
  std::string tag = "tag1";
  syncer::SyncChangeList changes;

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  manager()->DeleteForeignSessionInternal(tag, &changes);
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  EXPECT_TRUE(changes.empty());

  // Fill an instance of session specifics with a foreign session's data.
  std::vector<sync_pb::SessionSpecifics> tabs;
  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_nums1(n1, n1 + arraysize(n1));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_nums1, &tabs));

  // Update associator with the session's meta node, window, and tabs.
  manager()->UpdateTrackerWithForeignSession(meta, base::Time());
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs.begin();
       iter != tabs.end(); ++iter) {
    manager()->UpdateTrackerWithForeignSession(*iter, base::Time());
  }
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  // Now delete the foreign session.
  manager()->DeleteForeignSessionInternal(tag, &changes);
  EXPECT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));

  EXPECT_EQ(5U, changes.size());
  std::set<std::string> expected_tags(&tag, &tag + 1);
  for (int i = 0; i < 5; i++)
    expected_tags.insert(TabNodePool::TabIdToTag(tag, i));

  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(changes[i].ToString());
    EXPECT_TRUE(changes[i].IsValid());
    EXPECT_EQ(SyncChange::ACTION_DELETE, changes[i].change_type());
    EXPECT_TRUE(changes[i].sync_data().IsValid());
    EXPECT_EQ(1U,
              expected_tags.erase(
                  syncer::SyncDataLocal(changes[i].sync_data()).GetTag()));
  }
}

// Write a foreign session to a node, with the tabs arriving first, and then
// retrieve it.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeTabsFirst) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  SessionID::id_type nums1[] = {5, 10, 13, 17};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list1(nums1, nums1 + arraysize(nums1));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_list1, &tabs1));

  syncer::SyncChangeList adds;
  // Add tabs for first window, then the meta node.
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &adds);
  adds.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, adds);

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session to a node with some tabs that never arrive.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeMissingTabs) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  SessionID::id_type nums1[] = {5, 10, 13, 17};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list1(nums1, nums1 + arraysize(nums1));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_list1, &tabs1));
  // Add a second window, but this time only create two tab nodes, despite the
  // window expecting four tabs.
  SessionID::id_type tab_nums2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(
      tab_nums2, tab_nums2 + arraysize(tab_nums2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i], &tabs2[i]);
  }

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  AddTabsToChangeList(tabs2, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[0]->windows.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(1)->second->tabs.size());

  // Close the second window.
  meta.mutable_header()->clear_window();
  helper()->AddWindowSpecifics(0, tab_list1, &meta);
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_UPDATE));
  // Update associator with the session's meta node containing one window.
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  // Check that the foreign session was associated and retrieve the data.
  foreign_sessions.clear();
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Tests that the SessionsSyncManager can handle a remote client deleting
// sync nodes that belong to this local session.
TEST_F(SessionsSyncManagerTest, ProcessRemoteDeleteOfLocalSession) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(2U, out.size());
  sync_pb::EntitySpecifics entity(out[0].sync_data().GetSpecifics());
  SyncData d(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  SetSyncData(syncer::SyncDataList(&d, &d + 1));
  out.clear();

  syncer::SyncChangeList changes;
  changes.push_back(
      MakeRemoteChange(1, entity.session(), SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(manager()->local_tab_pool_out_of_sync_);
  EXPECT_TRUE(out.empty());  // ChangeProcessor shouldn't see any activity.

  // This should trigger repair of the TabNodePool.
  const GURL foo1("http://foo/1");
  AddTab(browser(), foo1);
  EXPECT_FALSE(manager()->local_tab_pool_out_of_sync_);

  // AddTab triggers two notifications, one for the tab insertion and one for
  // committing the NavigationEntry. The first notification results in a tab
  // we don't associate although we do update the header node.  The second
  // notification triggers association of the tab, and the subsequent window
  // update.  So we should see 4 changes at the SyncChangeProcessor.
  ASSERT_EQ(4U, out.size());

  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[0].change_type());
  ASSERT_TRUE(out[0].sync_data().GetSpecifics().session().has_header());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[1].change_type());
  int tab_node_id = out[1].sync_data().GetSpecifics().session().tab_node_id();
  EXPECT_EQ(TabNodePool::TabIdToTag(
                manager()->current_machine_tag(), tab_node_id),
            syncer::SyncDataLocal(out[1].sync_data()).GetTag());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[2].change_type());
  ASSERT_TRUE(out[2].sync_data().GetSpecifics().session().has_tab());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[3].change_type());
  ASSERT_TRUE(out[3].sync_data().GetSpecifics().session().has_header());

  // Verify the actual content.
  const sync_pb::SessionHeader& session_header =
      out[3].sync_data().GetSpecifics().session().header();
  ASSERT_EQ(1, session_header.window_size());
  EXPECT_EQ(1, session_header.window(0).tab_size());
  const sync_pb::SessionTab& tab1 =
      out[2].sync_data().GetSpecifics().session().tab();
  ASSERT_EQ(1, tab1.navigation_size());
  EXPECT_EQ(foo1.spec(), tab1.navigation(0).virtual_url());

  // Verify TabNodePool integrity.
  EXPECT_EQ(1U, manager()->local_tab_pool_.Capacity());
  EXPECT_TRUE(manager()->local_tab_pool_.Empty());

  // Verify TabLinks.
  SessionsSyncManager::TabLinksMap tab_map = manager()->local_tab_map_;
  ASSERT_EQ(1U, tab_map.size());
  int tab_id = out[2].sync_data().GetSpecifics().session().tab().tab_id();
  EXPECT_EQ(tab_node_id, tab_map.find(tab_id)->second->tab_node_id());
}

// Test that receiving a session delete from sync removes the session
// from tracking.
TEST_F(SessionsSyncManagerTest, ProcessForeignDelete) {
  InitWithNoSyncData();
  SessionID::id_type n[] = {5};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      "tag1", tab_list, &tabs1));

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  changes.clear();
  foreign_sessions.clear();
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabs) {
  syncer::SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(15);
  std::string session_tag = "tag1";

  // 0 will not have ownership changed.
  // 1 will not be updated, but header will stop owning.
  // 2 will be deleted before header stops owning.
  // 3 will be deleted after header stops owning.
  // 4 will be deleted before header update, but header will still try to own.
  // 5 will be deleted after header update, but header will still try to own.
  // 6 starts orphaned and then deleted before header update.
  // 7 starts orphaned and then deleted after header update.
  SessionID::id_type tab_ids[] = {0, 1, 2, 3, 4, 5};
  std::vector<SessionID::id_type> tab_list(tab_ids,
                                           tab_ids + arraysize(tab_ids));
  std::vector<sync_pb::SessionSpecifics> tabs;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(session_tag, tab_list, &tabs));
  AddToSyncDataList(meta, &foreign_data, stale_mtime);
  AddTabsToSyncDataList(tabs, &foreign_data);
  sync_pb::SessionSpecifics orphan6;
  helper()->BuildTabSpecifics(session_tag, 0, 6, &orphan6);
  AddToSyncDataList(orphan6, &foreign_data, stale_mtime);
  sync_pb::SessionSpecifics orphan7;
  helper()->BuildTabSpecifics(session_tag, 0, 7, &orphan7);
  AddToSyncDataList(orphan7, &foreign_data, stale_mtime);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  SessionID::id_type update_ids[] = {0, 4, 5};
  std::vector<SessionID::id_type> update_list(
      update_ids, update_ids + arraysize(update_ids));
  sync_pb::SessionWindow* window = meta.mutable_header()->mutable_window(0);
  window->clear_tab();
  for (int i : update_ids) {
    window->add_tab(i);
  }

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, tabs[2], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, tabs[4], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, orphan6, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_UPDATE));
  changes.push_back(MakeRemoteChange(1, tabs[3], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, tabs[5], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, orphan7, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(update_list);
  helper()->VerifySyncedSession(session_tag, session_reference,
                                *(foreign_sessions[0]));

  // Everything except for session, tab0, and tab1 will have no node_id, and
  // should get skipped by garbage collection.
  manager()->DoGarbageCollection();
  ASSERT_EQ(3U, output.size());
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabsWithShadowing) {
  syncer::SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(16);
  std::string session_tag = "tag1";

  // Add several tabs that shadow eachother, in that they share tab_ids. They
  // will, thanks to the helper, have unique tab_node_ids.
  sync_pb::SessionSpecifics tab1A;
  helper()->BuildTabSpecifics(session_tag, 0, 1, &tab1A);
  AddToSyncDataList(tab1A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab1B;
  helper()->BuildTabSpecifics(session_tag, 0, 1, &tab1B);
  AddToSyncDataList(tab1B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab1C;
  helper()->BuildTabSpecifics(session_tag, 0, 1, &tab1C);
  AddToSyncDataList(tab1C, &foreign_data, stale_mtime);

  sync_pb::SessionSpecifics tab2A;
  helper()->BuildTabSpecifics(session_tag, 0, 2, &tab2A);
  AddToSyncDataList(tab2A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab2B;
  helper()->BuildTabSpecifics(session_tag, 0, 2, &tab2B);
  AddToSyncDataList(tab2B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab2C;
  helper()->BuildTabSpecifics(session_tag, 0, 2, &tab2C);
  AddToSyncDataList(tab2C, &foreign_data, stale_mtime);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Verify that cleanup post-merge cleanup correctly removes all tabs objects.
  const sessions::SessionTab* tab;
  ASSERT_FALSE(
      manager()->session_tracker_.LookupSessionTab(session_tag, 1, &tab));
  ASSERT_FALSE(
      manager()->session_tracker_.LookupSessionTab(session_tag, 2, &tab));

  std::set<int> tab_node_ids;
  manager()->session_tracker_.LookupTabNodeIds(session_tag, &tab_node_ids);
  EXPECT_EQ(6U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab1A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab1B.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab1C.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2B.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2C.tab_node_id()) != tab_node_ids.end());

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, tab1A, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, tab1B, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(1, tab2C, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  tab_node_ids.clear();
  manager()->session_tracker_.LookupTabNodeIds(session_tag, &tab_node_ids);
  EXPECT_EQ(3U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab1C.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2B.tab_node_id()) != tab_node_ids.end());

  manager()->DoGarbageCollection();
  ASSERT_EQ(3U, output.size());
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabsWithReusedNodeIds) {
  syncer::SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(16);
  std::string session_tag = "tag1";
  int tab_node_id_shared = 13;
  int tab_node_id_unique = 14;

  sync_pb::SessionSpecifics tab1A;
  helper()->BuildTabSpecifics(session_tag, 0, 1, tab_node_id_shared, &tab1A);
  AddToSyncDataList(tab1A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab1B;
  helper()->BuildTabSpecifics(session_tag, 0, 1, tab_node_id_unique, &tab1B);
  AddToSyncDataList(tab1B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab2A;
  helper()->BuildTabSpecifics(session_tag, 0, 2, tab_node_id_shared, &tab2A);
  AddToSyncDataList(tab2A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  std::set<int> tab_node_ids;
  manager()->session_tracker_.LookupTabNodeIds(session_tag, &tab_node_ids);
  EXPECT_EQ(2U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_shared) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_unique) != tab_node_ids.end());

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, tab1A, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  tab_node_ids.clear();
  manager()->session_tracker_.LookupTabNodeIds(session_tag, &tab_node_ids);
  EXPECT_EQ(1U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_unique) != tab_node_ids.end());

  manager()->DoGarbageCollection();
  EXPECT_EQ(1U, output.size());
}

// TODO(shashishekhar): "Move this to TabNodePool unittests."
TEST_F(SessionsSyncManagerTest, SaveUnassociatedNodesForReassociation) {
  syncer::SyncChangeList changes;
  InitWithNoSyncData();

  std::string local_tag = manager()->current_machine_tag();
  // Create a free node and then dissassociate sessions so that it ends up
  // unassociated.
  manager()->local_tab_pool_.GetFreeTabNode(&changes);

  // Update the tab_id of the node, so that it is considered a valid
  // unassociated node otherwise it will be mistaken for a corrupted node and
  // will be deleted before being added to the tab node pool.
  sync_pb::EntitySpecifics entity(changes[0].sync_data().GetSpecifics());
  entity.mutable_session()->mutable_tab()->set_tab_id(1);
  SyncData d(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  syncer::SyncDataList in(&d, &d + 1);
  changes.clear();
  SessionsSyncManager manager2(GetSyncSessionsClient(), sync_prefs(),
                               local_device(), NewDummyRouter(),
                               base::Closure(), base::Closure());
  syncer::SyncMergeResult result = manager2.MergeDataAndStartSyncing(
      syncer::SESSIONS, in, std::unique_ptr<syncer::SyncChangeProcessor>(
                                new TestSyncProcessorStub(&changes)),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));
  ASSERT_FALSE(result.error().IsSet());
  EXPECT_TRUE(FilterOutLocalHeaderChanges(&changes)->empty());
}

TEST_F(SessionsSyncManagerTest, MergeDeletesCorruptNode) {
  syncer::SyncChangeList changes;
  InitWithNoSyncData();

  std::string local_tag = manager()->current_machine_tag();
  int tab_node_id = manager()->local_tab_pool_.GetFreeTabNode(&changes);
  SyncData d(SyncData::CreateRemoteData(
      1,
      changes[0].sync_data().GetSpecifics(),
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  syncer::SyncDataList in(&d, &d + 1);
  changes.clear();
  TearDown();
  SetUp();
  InitWithSyncDataTakeOutput(in, &changes);
  EXPECT_EQ(1U, FilterOutLocalHeaderChanges(&changes)->size());
  EXPECT_EQ(SyncChange::ACTION_DELETE, changes[0].change_type());
  EXPECT_EQ(TabNodePool::TabIdToTag(local_tag, tab_node_id),
            syncer::SyncDataLocal(changes[0].sync_data()).GetTag());
}

// Verifies that we drop both headers and tabs during merge if their stored tag
// hash doesn't match a computer tag hash. This mitigates potential failures
// while cleaning up bad foreign data, see crbug.com/604657.
TEST_F(SessionsSyncManagerTest, MergeDeletesBadHash) {
  syncer::SyncDataList foreign_data;
  std::vector<SessionID::id_type> empty_ids;
  std::vector<sync_pb::SessionSpecifics> empty_tabs;
  sync_pb::EntitySpecifics entity;

  const std::string good_header_tag = "good_header_tag";
  sync_pb::SessionSpecifics good_header(
      helper()->BuildForeignSession(good_header_tag, empty_ids, &empty_tabs));
  foreign_data.push_back(CreateRemoteData(good_header));

  const std::string bad_header_tag = "bad_header_tag";
  sync_pb::SessionSpecifics bad_header(
      helper()->BuildForeignSession(bad_header_tag, empty_ids, &empty_tabs));
  entity.mutable_session()->CopyFrom(bad_header);
  foreign_data.push_back(SyncData::CreateRemoteData(
      1, entity, base::Time(), syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create(), "bad_header_tag_hash"));

  const std::string good_tag_tab = "good_tag_tab";
  sync_pb::SessionSpecifics good_tab;
  helper()->BuildTabSpecifics(good_tag_tab, 0, 1, &good_tab);
  foreign_data.push_back(CreateRemoteData(good_tab));

  const std::string bad_tab_tag = "bad_tab_tag";
  sync_pb::SessionSpecifics bad_tab;
  helper()->BuildTabSpecifics(bad_tab_tag, 0, 2, &bad_tab);
  entity.mutable_session()->CopyFrom(bad_tab);
  foreign_data.push_back(SyncData::CreateRemoteData(
      1, entity, base::Time(), syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create(), "bad_tab_tag_hash"));

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, FilterOutLocalHeaderChanges(&output)->size());
  ExpectAllOfChangesType(output, SyncChange::ACTION_DELETE);
  EXPECT_EQ(1, CountIfTagMatches(output, bad_header_tag));
  EXPECT_EQ(1, CountIfTagMatches(output, bad_tab_tag));

  std::vector<const sync_driver::SyncedSession*> sessions;
  manager()->session_tracker_.LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW);
  ASSERT_EQ(2U, sessions.size());
  EXPECT_EQ(1, CountIfTagMatches(sessions, good_header_tag));
  EXPECT_EQ(1, CountIfTagMatches(sessions, good_tag_tab));
}

// Test that things work if a tab is initially ignored.
TEST_F(SessionsSyncManagerTest, AssociateWindowsDontReloadTabs) {
  syncer::SyncChangeList out;
  // Go to a URL that is ignored by session syncing.
  AddTab(browser(), GURL("chrome://preferences/"));
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(2U, out.size());  // Header add and update.
  EXPECT_EQ(
      0,
      out[1].sync_data().GetSpecifics().session().header().window_size());
  out.clear();

  // Go to a sync-interesting URL.
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  EXPECT_EQ(3U, out.size());  // Tab add, update, and header update.

  EXPECT_TRUE(
      base::StartsWith(syncer::SyncDataLocal(out[0].sync_data()).GetTag(),
                       manager()->current_machine_tag(),
                       base::CompareCase::SENSITIVE));
  EXPECT_EQ(manager()->current_machine_tag(),
            out[0].sync_data().GetSpecifics().session().session_tag());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[0].change_type());

  EXPECT_TRUE(
      base::StartsWith(syncer::SyncDataLocal(out[1].sync_data()).GetTag(),
                       manager()->current_machine_tag(),
                       base::CompareCase::SENSITIVE));
  EXPECT_EQ(manager()->current_machine_tag(),
            out[1].sync_data().GetSpecifics().session().session_tag());
  EXPECT_TRUE(out[1].sync_data().GetSpecifics().session().has_tab());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[1].change_type());

  EXPECT_TRUE(out[2].IsValid());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[2].change_type());
  const SyncData data(out[2].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data).GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_EQ(1, header_s.window_size());
  EXPECT_EQ(1, header_s.window(0).tab_size());
}

// Tests that the SyncSessionManager responds to local tab events properly.
TEST_F(SessionsSyncManagerTest, OnLocalTabModified) {
  syncer::SyncChangeList out;
  // Init with no local data, relies on MergeLocalSessionNoTabs.
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_FALSE(manager()->current_machine_tag().empty());
  ASSERT_EQ(2U, out.size());

  // Copy the original header.
  sync_pb::EntitySpecifics header(out[0].sync_data().GetSpecifics());
  out.clear();

  const GURL foo1("http://foo/1");
  const GURL foo2("http://foo/2");
  const GURL bar1("http://bar/1");
  const GURL bar2("http://bar/2");
  AddTab(browser(), foo1);
  NavigateAndCommitActiveTab(foo2);
  AddTab(browser(), bar1);
  NavigateAndCommitActiveTab(bar2);

  // One add, one update for each AddTab.
  // One update for each NavigateAndCommit.
  // = 6 total tab updates.
  // One header update corresponding to each of those.
  // = 6 total header updates.
  // 12 total updates.
  ASSERT_EQ(12U, out.size());

  // Verify the tab node creations and updates to ensure the SyncProcessor
  // sees the right operations.
  for (int i = 0; i < 12; i++) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(out[i].IsValid());
    const SyncData data(out[i].sync_data());
    EXPECT_TRUE(base::StartsWith(syncer::SyncDataLocal(data).GetTag(),
                                 manager()->current_machine_tag(),
                                 base::CompareCase::SENSITIVE));
    const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
    EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
    if (i % 6 == 0) {
      // First thing on an AddTab is a no-op header update for parented tab.
      EXPECT_EQ(header.SerializeAsString(),
                data.GetSpecifics().SerializeAsString());
      EXPECT_EQ(manager()->current_machine_tag(),
                syncer::SyncDataLocal(data).GetTag());
    } else if (i % 6 == 1) {
      // Next, the TabNodePool should create the tab node.
      EXPECT_EQ(SyncChange::ACTION_ADD, out[i].change_type());
      EXPECT_EQ(TabNodePool::TabIdToTag(
                    manager()->current_machine_tag(),
                    data.GetSpecifics().session().tab_node_id()),
                syncer::SyncDataLocal(data).GetTag());
    } else if (i % 6 == 2) {
      // Then we see the tab update to the URL.
      EXPECT_EQ(SyncChange::ACTION_UPDATE, out[i].change_type());
      EXPECT_EQ(TabNodePool::TabIdToTag(
                    manager()->current_machine_tag(),
                    data.GetSpecifics().session().tab_node_id()),
                syncer::SyncDataLocal(data).GetTag());
      ASSERT_TRUE(specifics.has_tab());
    } else if (i % 6 == 3) {
      // The header needs to be updated to reflect the new window state.
      EXPECT_EQ(SyncChange::ACTION_UPDATE, out[i].change_type());
      EXPECT_TRUE(specifics.has_header());
    } else if (i % 6 == 4) {
      // Now we move on to NavigateAndCommit.  Update the tab.
      EXPECT_EQ(SyncChange::ACTION_UPDATE, out[i].change_type());
      EXPECT_EQ(TabNodePool::TabIdToTag(
                    manager()->current_machine_tag(),
                    data.GetSpecifics().session().tab_node_id()),
                syncer::SyncDataLocal(data).GetTag());
      ASSERT_TRUE(specifics.has_tab());
    } else if (i % 6 == 5) {
      // The header needs to be updated to reflect the new window state.
      EXPECT_EQ(SyncChange::ACTION_UPDATE, out[i].change_type());
      ASSERT_TRUE(specifics.has_header());
      header = data.GetSpecifics();
    }
  }

  // Verify the actual content to ensure sync sees the right data.
  // When it's all said and done, the header should reflect two tabs.
  const sync_pb::SessionHeader& session_header = header.session().header();
  ASSERT_EQ(1, session_header.window_size());
  EXPECT_EQ(2, session_header.window(0).tab_size());

  // ASSERT_TRUEs above allow us to dive in freely here.
  // Verify first tab.
  const sync_pb::SessionTab& tab1_1 =
      out[2].sync_data().GetSpecifics().session().tab();
  ASSERT_EQ(1, tab1_1.navigation_size());
  EXPECT_EQ(foo1.spec(), tab1_1.navigation(0).virtual_url());
  const sync_pb::SessionTab& tab1_2 =
      out[4].sync_data().GetSpecifics().session().tab();
  ASSERT_EQ(2, tab1_2.navigation_size());
  EXPECT_EQ(foo1.spec(), tab1_2.navigation(0).virtual_url());
  EXPECT_EQ(foo2.spec(), tab1_2.navigation(1).virtual_url());

  // Verify second tab.
  const sync_pb::SessionTab& tab2_1 =
      out[8].sync_data().GetSpecifics().session().tab();
  ASSERT_EQ(1, tab2_1.navigation_size());
  EXPECT_EQ(bar1.spec(), tab2_1.navigation(0).virtual_url());
  const sync_pb::SessionTab& tab2_2 =
      out[10].sync_data().GetSpecifics().session().tab();
  ASSERT_EQ(2, tab2_2.navigation_size());
  EXPECT_EQ(bar1.spec(), tab2_2.navigation(0).virtual_url());
  EXPECT_EQ(bar2.spec(), tab2_2.navigation(1).virtual_url());
}

// Check that if a tab becomes uninteresting (for example no syncable URLs),
// we correctly remove it from the header node.
TEST_F(SessionsSyncManagerTest, TabBecomesUninteresting) {
  syncer::SyncChangeList out;
  // Init with no local data, relies on MergeLocalSessionNoTabs.
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_FALSE(manager()->current_machine_tag().empty());
  ASSERT_EQ(2U, out.size());
  out.clear();

  const GURL kValidUrl("http://foo/1");
  const GURL kInternalUrl("chrome://internal");

  // Add an interesting tab.
  AddTab(browser(), kValidUrl);
  // No-op header update, tab creation, tab update, header update.
  ASSERT_EQ(4U, out.size());
  // The last two are the interesting updates.
  ASSERT_TRUE(out[2].sync_data().GetSpecifics().session().has_tab());
  EXPECT_EQ(kValidUrl.spec(), out[2]
                                  .sync_data()
                                  .GetSpecifics()
                                  .session()
                                  .tab()
                                  .navigation(0)
                                  .virtual_url());
  ASSERT_TRUE(out[3].sync_data().GetSpecifics().session().has_header());
  ASSERT_EQ(1,
            out[3].sync_data().GetSpecifics().session().header().window_size());
  ASSERT_EQ(1, out[3]
                   .sync_data()
                   .GetSpecifics()
                   .session()
                   .header()
                   .window(0)
                   .tab_size());

  // Navigate five times to uninteresting urls to push the interesting one off
  // the back of the stack.
  NavigateAndCommitActiveTab(kInternalUrl);
  NavigateAndCommitActiveTab(kInternalUrl);
  NavigateAndCommitActiveTab(kInternalUrl);
  NavigateAndCommitActiveTab(kInternalUrl);

  // Reset |out| so we only see the effects of the final navigation.
  out.clear();
  NavigateAndCommitActiveTab(kInternalUrl);

  // Only the header node should be updated, and it should no longer have any
  // valid windows/tabs.
  ASSERT_EQ(2U, out.size());  // Two header updates (first is a no-op).
  ASSERT_TRUE(out[1].sync_data().GetSpecifics().session().has_header());
  EXPECT_EQ(1,
            out[1].sync_data().GetSpecifics().session().header().window_size());
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionExistingTabs) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));  // Adds back entry.
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));  // Adds back entry.

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(6U, out.size());

  // Check that this machine's data is not included in the foreign windows.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));

  // Verify the header.
  EXPECT_TRUE(out[0].IsValid());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[0].change_type());
  const SyncData data(out[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data).GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_TRUE(header_s.has_device_type());
  EXPECT_EQ(GetLocalDeviceInfo()->client_name(), header_s.client_name());
  EXPECT_EQ(0, header_s.window_size());

  // Verify the tab node creations and updates with content.
  for (int i = 1; i < 5; i++) {
    EXPECT_TRUE(out[i].IsValid());
    const SyncData data(out[i].sync_data());
    EXPECT_TRUE(base::StartsWith(syncer::SyncDataLocal(data).GetTag(),
                                 manager()->current_machine_tag(),
                                 base::CompareCase::SENSITIVE));
    const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
    EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
    if (i % 2 == 1) {
      EXPECT_EQ(SyncChange::ACTION_ADD, out[i].change_type());
    } else {
      EXPECT_EQ(SyncChange::ACTION_UPDATE, out[i].change_type());
      EXPECT_TRUE(specifics.has_tab());
    }
  }

  // Verify the header was updated to reflect new window state.
  EXPECT_TRUE(out[5].IsValid());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[5].change_type());
  const SyncData data_2(out[5].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(),
            syncer::SyncDataLocal(data_2).GetTag());
  const sync_pb::SessionSpecifics& specifics2(data_2.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics2.session_tag());
  EXPECT_TRUE(specifics2.has_header());
  const sync_pb::SessionHeader& header_s2 = specifics2.header();
  EXPECT_EQ(1, header_s2.window_size());

  // Verify TabLinks.
  SessionsSyncManager::TabLinksMap tab_map = manager()->local_tab_map_;
  ASSERT_EQ(2U, tab_map.size());
  // Tabs are ordered by sessionid in tab_map, so should be able to traverse
  // the tree based on order of tabs created
  SessionsSyncManager::TabLinksMap::iterator iter = tab_map.begin();
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  EXPECT_EQ(GURL("http://foo1"), iter->second->tab()->GetVirtualURLAtIndex(0));
  EXPECT_EQ(GURL("http://foo2"), iter->second->tab()->GetVirtualURLAtIndex(1));
  iter++;
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  EXPECT_EQ(GURL("http://bar1"), iter->second->tab()->GetVirtualURLAtIndex(0));
  EXPECT_EQ(GURL("http://bar2"), iter->second->tab()->GetVirtualURLAtIndex(1));
}

TEST_F(SessionsSyncManagerTest, ForeignSessionModifiedTime) {
  syncer::SyncDataList foreign_data;
  base::Time newest_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  base::Time middle_time = base::Time::Now() - base::TimeDelta::FromDays(2);
  base::Time oldest_time = base::Time::Now() - base::TimeDelta::FromDays(3);

  {
    std::string session_tag = "tag1";
    SessionID::id_type n[] = {1, 2};
    std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, tab_list, &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, newest_time);
    AddToSyncDataList(meta, &foreign_data, middle_time);
    AddToSyncDataList(tabs[1], &foreign_data, oldest_time);
  }

  {
    std::string session_tag = "tag2";
    SessionID::id_type n[] = {3, 4};
    std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, tab_list, &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, middle_time);
    AddToSyncDataList(meta, &foreign_data, newest_time);
    AddToSyncDataList(tabs[1], &foreign_data, oldest_time);
  }

  {
    std::string session_tag = "tag3";
    SessionID::id_type n[] = {5, 6};
    std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, tab_list, &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, oldest_time);
    AddToSyncDataList(meta, &foreign_data, middle_time);
    AddToSyncDataList(tabs[1], &foreign_data, newest_time);
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(3U, foreign_sessions.size());
  EXPECT_EQ(newest_time, foreign_sessions[0]->modified_time);
  EXPECT_EQ(newest_time, foreign_sessions[1]->modified_time);
  EXPECT_EQ(newest_time, foreign_sessions[2]->modified_time);
}

// Test garbage collection of stale foreign sessions.
TEST_F(SessionsSyncManagerTest, DoGarbageCollection) {
  // Fill two instances of session specifics with a foreign session's data.
  std::string tag1 = "tag1";
  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag1, tab_list1, &tabs1));
  std::string tag2 = "tag2";
  SessionID::id_type n2[] = {8, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(n2, n2 + arraysize(n2));
  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(helper()->BuildForeignSession(
      tag2, tab_list2, &tabs2));
  // Set the modification time for tag1 to be 21 days ago, tag2 to 5 days ago.
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  base::Time tag2_time = base::Time::Now() - base::TimeDelta::FromDays(5);

  syncer::SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta, 1, tag1_time));
  foreign_data.push_back(CreateRemoteData(meta2, 1, tag2_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  AddTabsToSyncDataList(tabs2, &foreign_data);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  foreign_sessions.clear();

  // Now garbage collect and verify the non-stale session is still there.
  manager()->DoGarbageCollection();
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ(SyncChange::ACTION_DELETE, output[0].change_type());
  const SyncData data(output[0].sync_data());
  EXPECT_EQ(tag1, syncer::SyncDataLocal(data).GetTag());
  for (int i = 1; i < 5; i++) {
    EXPECT_EQ(SyncChange::ACTION_DELETE, output[i].change_type());
    const SyncData data(output[i].sync_data());
    EXPECT_EQ(TabNodePool::TabIdToTag(tag1, i),
              syncer::SyncDataLocal(data).GetTag());
  }

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list2);
  helper()->VerifySyncedSession(tag2, session_reference,
                                *(foreign_sessions[0]));
}

TEST_F(SessionsSyncManagerTest, DoGarbageCollectionOrphans) {
  syncer::SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(15);

  {
    // A stale session with empty header
    std::string session_tag = "tag1";
    std::vector<SessionID::id_type> tab_list;
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, tab_list, &tabs));
    AddToSyncDataList(meta, &foreign_data, stale_mtime);
  }

  {
    // A stale session with orphans w/o header
    std::string session_tag = "tag2";
    sync_pb::SessionSpecifics orphan;
    helper()->BuildTabSpecifics(session_tag, 0, 1, &orphan);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);
  }

  {
    // A stale session with valid header/tab and an orphaned tab.
    std::string session_tag = "tag3";
    SessionID::id_type n[] = {2};
    std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, tab_list, &tabs));

    // BuildForeignSession(...) will use a window id of 0, and we're also
    // passing a window id of 0 to BuildTabSpecifics(...) here.  It doesn't
    // really matter what window id we use for the orphaned tab, in the real
    // world orphans often reference real/still valid windows, but they're
    // orphans because the window/header doesn't reference back to them.
    sync_pb::SessionSpecifics orphan;
    helper()->BuildTabSpecifics(session_tag, 0, 1, &orphan);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);

    AddToSyncDataList(tabs[0], &foreign_data, stale_mtime);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);
    AddToSyncDataList(meta, &foreign_data, stale_mtime);
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Although we have 3 foreign sessions, only 1 is valid/clean enough.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  foreign_sessions.clear();

  // Everything should get removed here.
  manager()->DoGarbageCollection();
  // Expect 5 deletions. tag1 header only, tag2 tab only, tag3 header + 2x tabs.
  ASSERT_EQ(5U, output.size());
}

// Test that an update to a previously considered "stale" session,
// prior to garbage collection, will save the session from deletion.
TEST_F(SessionsSyncManagerTest, GarbageCollectionHonoursUpdate) {
  std::string tag1 = "tag1";
  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag1, tab_list1, &tabs1));
  syncer::SyncDataList foreign_data;
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  foreign_data.push_back(CreateRemoteData(meta, 1, tag1_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());

  // Update to a non-stale time.
  sync_pb::EntitySpecifics update_entity;
  update_entity.mutable_session()->CopyFrom(tabs1[0]);
  syncer::SyncChangeList changes;
  changes.push_back(
      syncer::SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                         CreateRemoteData(tabs1[0], 1, base::Time::Now())));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  foreign_sessions.clear();

  // Verify the now non-stale session does not get deleted.
  manager()->DoGarbageCollection();
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(
      tag1, session_reference, *(foreign_sessions[0]));
}

// Test that swapping WebContents for a tab is properly observed and handled
// by the SessionsSyncManager.
TEST_F(SessionsSyncManagerTest, CheckPrerenderedWebContentsSwap) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(4U, out.size());  // Header, tab ADD, tab UPDATE, header UPDATE.

  // To simulate WebContents swap during prerendering, create new WebContents
  // and swap with old WebContents.
  std::unique_ptr<content::WebContents> old_web_contents;
  old_web_contents.reset(browser()->tab_strip_model()->GetActiveWebContents());

  // Create new WebContents, with the required tab helpers.
  WebContents* new_web_contents = WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(profile()),
      old_web_contents->GetController().GetSessionStorageNamespaceMap());
  SessionTabHelper::CreateForWebContents(new_web_contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(new_web_contents);
  new_web_contents->GetController()
      .CopyStateFrom(old_web_contents->GetController());

  // Swap the WebContents.
  int index = browser()->tab_strip_model()->GetIndexOfWebContents(
      old_web_contents.get());
  browser()->tab_strip_model()->ReplaceWebContentsAt(index, new_web_contents);

  ASSERT_EQ(9U, out.size());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[4].change_type());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, out[5].change_type());

  // Navigate away.
  NavigateAndCommitActiveTab(GURL("http://bar2"));

  // Delete old WebContents. This should not crash.
  old_web_contents.reset();

  // Try more navigations and verify output size. This can also reveal
  // bugs (leaks) on memcheck bots if the SessionSyncManager
  // didn't properly clean up the tab pool or session tracker.
  NavigateAndCommitActiveTab(GURL("http://bar3"));

  AddTab(browser(), GURL("http://bar4"));
  NavigateAndCommitActiveTab(GURL("http://bar5"));
  ASSERT_EQ(19U, out.size());
}

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when processing
// sync changes.
TEST_F(SessionsSyncManagerTest, NotifiedOfUpdates) {
  ASSERT_FALSE(observer()->notified_of_update());
  InitWithNoSyncData();

  SessionID::id_type n[] = {5};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      "tag1", tab_list, &tabs1));

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer()->notified_of_update());

  changes.clear();
  observer()->Reset();
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer()->notified_of_update());

  changes.clear();
  observer()->Reset();
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer()->notified_of_update());
}

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when handling
// local hide/removal of foreign session.
TEST_F(SessionsSyncManagerTest, NotifiedOfLocalRemovalOfForeignSession) {
  InitWithNoSyncData();
  const std::string tag("tag1");
  SessionID::id_type n[] = {5};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      tag, tab_list, &tabs1));

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  observer()->Reset();
  ASSERT_FALSE(observer()->notified_of_update());
  manager()->DeleteForeignSession(tag);
  ASSERT_TRUE(observer()->notified_of_update());
}

#if defined(OS_ANDROID)
// Tests that opening the other devices page triggers a session sync refresh.
// This page only exists on mobile platforms today; desktop has a
// search-enhanced NTP without other devices.
TEST_F(SessionsSyncManagerTest, NotifiedOfRefresh) {
  ASSERT_FALSE(observer()->notified_of_refresh());
  InitWithNoSyncData();
  AddTab(browser(), GURL("http://foo1"));
  EXPECT_FALSE(observer()->notified_of_refresh());
  NavigateAndCommitActiveTab(GURL("chrome://newtab/#open_tabs"));
  EXPECT_TRUE(observer()->notified_of_refresh());
}
#endif  // defined(OS_ANDROID)

// Tests receipt of duplicate tab IDs in the same window.  This should never
// happen, but we want to make sure the client won't do anything bad if it does
// receive such garbage input data.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInSameWindow) {
  std::string tag = "tag1";

  // Reuse tab ID 10 in an attempt to trigger bad behavior.
  SessionID::id_type n1[] = {5, 10, 10, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));

  // Set up initial data.
  syncer::SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  AddTabsToSyncDataList(tabs1, &initial_data);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of duplicate tab IDs for the same session.  The duplicate tab
// ID is present in two different windows.  A client can't be expected to do
// anything reasonable with this input, but we can expect that it doesn't
// crash.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInOtherWindow) {
  std::string tag = "tag1";

  SessionID::id_type n1[] = {5, 10, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));

  // Add a second window.  Tab ID 10 is a duplicate.
  SessionID::id_type n2[] = {10, 18, 20};
  std::vector<SessionID::id_type> tab_list2(n2, n2 + arraysize(n2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  syncer::SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  AddTabsToSyncDataList(tabs1, &initial_data);

  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i], entity.mutable_session());
    initial_data.push_back(SyncData::CreateRemoteData(
        i + 10,
        entity,
        base::Time(),
        syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create()));
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of multiple unassociated tabs and makes sure that
// the ones with later timestamp win
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateUnassociatedTabs) {
  std::string tag = "tag1";

  SessionID::id_type n1[] = {5, 10, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));

  // Set up initial data.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  int node_id = 2;
  sync_pb::EntitySpecifics entity;

  for (size_t i = 0; i < tabs1.size(); i++) {
    entity.mutable_session()->CopyFrom(tabs1[i]);
    initial_data.push_back(
        CreateRemoteData(entity, node_id++, base::Time::FromDoubleT(2000)));
  }

  // Add two more tabs with duplicating IDs but with different modification
  // times, one before and one after the tabs above.
  // These two tabs get a different visual indices to distinguish them from the
  // tabs above that get visual index 1 by default.
  sync_pb::SessionSpecifics duplicating_tab1;
  helper()->BuildTabSpecifics(tag, 0, 10, &duplicating_tab1);
  duplicating_tab1.mutable_tab()->set_tab_visual_index(2);
  entity.mutable_session()->CopyFrom(duplicating_tab1);
  initial_data.push_back(
      CreateRemoteData(entity, node_id++, base::Time::FromDoubleT(1000)));

  sync_pb::SessionSpecifics duplicating_tab2;
  helper()->BuildTabSpecifics(tag, 0, 17, &duplicating_tab2);
  duplicating_tab2.mutable_tab()->set_tab_visual_index(3);
  entity.mutable_session()->CopyFrom(duplicating_tab2);
  initial_data.push_back(
      CreateRemoteData(entity, node_id++, base::Time::FromDoubleT(3000)));

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));

  const std::vector<sessions::SessionTab*>& window_tabs =
      foreign_sessions[0]->windows.find(0)->second->tabs;
  ASSERT_EQ(3U, window_tabs.size());
  // The first one is from the original set of tabs.
  ASSERT_EQ(1, window_tabs[0]->tab_visual_index);
  // The one from the original set of tabs wins over duplicating_tab1.
  ASSERT_EQ(1, window_tabs[1]->tab_visual_index);
  // duplicating_tab2 wins due to the later timestamp.
  ASSERT_EQ(3, window_tabs[2]->tab_visual_index);
}

// Verify that GetAllForeignSessions returns all sessions sorted by recency.
TEST_F(SessionsSyncManagerTest, GetAllForeignSessions) {
  SessionID::id_type ids[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list(ids, ids + arraysize(ids));

  const std::string kTag = "tag1";
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta1(helper()->BuildForeignSession(
      kTag, tab_list, &tabs1));

  const std::string kTag2 = "tag2";
  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(helper()->BuildForeignSession(
      kTag2, tab_list, &tabs2));

  syncer::SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteData(meta1, 1, base::Time::FromInternalValue(10)));
  AddTabsToSyncDataList(tabs1, &initial_data);
  initial_data.push_back(
      CreateRemoteData(meta2, 2, base::Time::FromInternalValue(200)));
  AddTabsToSyncDataList(tabs2, &initial_data);

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_GT(foreign_sessions[0]->modified_time,
            foreign_sessions[1]->modified_time);
}

// Verify that GetForeignSessionTabs returns all tabs for a session sorted
// by recency.
TEST_F(SessionsSyncManagerTest, GetForeignSessionTabs) {
  const std::string kTag = "tag1";

  SessionID::id_type n1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(n1, n1 + arraysize(n1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      kTag, tab_list1, &tabs1));
  // Add a second window.
  SessionID::id_type n2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(n2, n2 + arraysize(n2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  // Add the first window's tabs.
  AddTabsToSyncDataList(tabs1, &initial_data);

  // Add the second window's tabs.
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag, 0, tab_list2[i],
                                entity.mutable_session());
    // Order the tabs oldest to most ReceiveDuplicateUnassociatedTabs and
    // left to right visually.
    initial_data.push_back(
        CreateRemoteData(entity, i + 10, base::Time::FromInternalValue(i + 1)));
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const sessions::SessionTab*> tabs;
  ASSERT_TRUE(manager()->GetForeignSessionTabs(kTag, &tabs));
  // Assert that the size matches the total number of tabs and that the order
  // is from most recent to least.
  ASSERT_EQ(tab_list1.size() + tab_list2.size(), tabs.size());
  base::Time last_time;
  for (size_t i = 0; i < tabs.size(); ++i) {
    base::Time this_time = tabs[i]->timestamp;
    if (i > 0)
      ASSERT_GE(last_time, this_time);
    last_time = tabs[i]->timestamp;
  }
}

}  // namespace browser_sync
