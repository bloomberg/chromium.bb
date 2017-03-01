// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <stdint.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/sync_sessions/session_sync_test_helper.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;
using syncer::DeviceInfo;
using syncer::LocalDeviceInfoProvider;
using syncer::LocalDeviceInfoProviderMock;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncDataLocal;
using syncer::SyncError;

namespace sync_sessions {

namespace {

const char kTitle[] = "title";
const char kFoo1[] = "http://foo1/";
const char kFoo2[] = "http://foo2/";
const char kBar1[] = "http://bar1/";
const char kBar2[] = "http://bar2/";
const char kBaz1[] = "http://baz1/";
const char kBaz2[] = "http://baz2/";
const char kTag1[] = "tag1";
const char kTag2[] = "tag2";
const int kTabIds1[] = {5, 10, 13, 17};
const int kTabIds2[] = {7, 15, 18, 20};

const base::Time kTime0 = base::Time::FromInternalValue(100);
const base::Time kTime1 = base::Time::FromInternalValue(110);
const base::Time kTime2 = base::Time::FromInternalValue(120);
const base::Time kTime3 = base::Time::FromInternalValue(130);
const base::Time kTime4 = base::Time::FromInternalValue(140);
const base::Time kTime5 = base::Time::FromInternalValue(150);
const base::Time kTime6 = base::Time::FromInternalValue(160);
const base::Time kTime7 = base::Time::FromInternalValue(170);
const base::Time kTime8 = base::Time::FromInternalValue(180);
const base::Time kTime9 = base::Time::FromInternalValue(190);

std::string TabNodeIdToTag(const std::string& machine_tag, int tab_node_id) {
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

size_t CountIfTagMatches(const SyncChangeList& changes,
                         const std::string& tag) {
  return std::count_if(
      changes.begin(), changes.end(), [&tag](const SyncChange& change) {
        return change.sync_data().GetSpecifics().session().session_tag() == tag;
      });
}

size_t CountIfTagMatches(const std::vector<const SyncedSession*>& sessions,
                         const std::string& tag) {
  return std::count_if(sessions.begin(), sessions.end(),
                       [&tag](const SyncedSession* session) {
                         return session->session_tag == tag;
                       });
}

testing::AssertionResult AllOfChangesAreType(
    const SyncChangeList& changes,
    const SyncChange::SyncChangeType type) {
  auto invalid_change = std::find_if(changes.begin(), changes.end(),
                                     [&type](const SyncChange& change) {
                                       return change.change_type() != type;
                                     });
  if (invalid_change != changes.end()) {
    return testing::AssertionFailure() << invalid_change->ToString()
                                       << " doesn't match "
                                       << SyncChange::ChangeTypeToString(type);
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ChangeTypeMatches(
    const SyncChangeList& changes,
    const std::vector<SyncChange::SyncChangeType>& types) {
  auto types_iter = types.begin();
  if (changes.size() != types.size() ||
      std::any_of(changes.begin(), changes.end(),
                  [&types_iter](const SyncChange& change) {
                    SCOPED_TRACE(change.ToString());
                    return change.change_type() != *types_iter++;
                  })) {
    std::string type_string;
    std::for_each(types.begin(), types.end(),
                  [&type_string](const SyncChange::SyncChangeType& type) {
                    (type_string) += SyncChange::ChangeTypeToString(type) + " ";
                  });
    std::string change_string;
    std::for_each(changes.begin(), changes.end(),
                  [&change_string](const SyncChange& change) {
                    change_string += change.ToString();
                  });
    return testing::AssertionFailure()
           << "Change type mismatch: " << type_string << " vs "
           << change_string;
  }
  return testing::AssertionSuccess();
}

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

// A SyncedTabDelegate fake for testing. It simulates a normal
// SyncedTabDelegate with a proper WebContents. For a SyncedTabDelegate without
// a WebContents, see PlaceholderTabDelegate below.
class TestSyncedTabDelegate : public SyncedTabDelegate {
 public:
  TestSyncedTabDelegate() {}
  ~TestSyncedTabDelegate() override {}

  // SyncedTabDelegate overrides.
  bool IsInitialBlankNavigation() const override {
    // This differs from NavigationControllerImpl, which has an initial blank
    // NavigationEntry.
    return GetEntryCount() == 0;
  }
  int GetCurrentEntryIndex() const override { return current_entry_index_; }
  GURL GetVirtualURLAtIndex(int i) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return GURL();
    return entries_[i]->virtual_url();
  }
  GURL GetFaviconURLAtIndex(int i) const override { return GURL(); }
  ui::PageTransition GetTransitionAtIndex(int i) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return ui::PAGE_TRANSITION_LINK;
    return entries_[i]->transition_type();
  }
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override {
    if (static_cast<size_t>(i) >= entries_.size())
      return;
    *serialized_entry = *entries_[i];
  }
  int GetEntryCount() const override { return entries_.size(); }
  SessionID::id_type GetWindowId() const override { return window_id_.id(); }
  SessionID::id_type GetSessionId() const override { return tab_id_.id(); }
  bool IsBeingDestroyed() const override { return false; }
  std::string GetExtensionAppId() const override { return std::string(); }
  bool ProfileIsSupervised() const override { return is_supervised_; }
  void set_is_supervised(bool is_supervised) { is_supervised_ = is_supervised; }
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override {
    return &blocked_navigations_;
  }
  bool IsPlaceholderTab() const override { return false; }
  int GetSyncId() const override { return sync_id_; }
  void SetSyncId(int sync_id) override { sync_id_ = sync_id; }
  bool ShouldSync(SyncSessionsClient* sessions_client) override {
    // This is just a simple filter that isn't meant to fully reproduce
    // the TabContentsTabDelegate's ShouldSync logic.
    // Verify all URL's are valid (which will ignore an initial blank page) and
    // that there is at least one http:// url.
    int http_count = 0;
    for (auto& entry : entries_) {
      if (!entry->virtual_url().is_valid())
        return false;
      if (entry->virtual_url().SchemeIsHTTPOrHTTPS())
        http_count++;
    }
    return http_count > 0;
  }

  void AppendEntry(std::unique_ptr<sessions::SerializedNavigationEntry> entry) {
    entries_.push_back(std::move(entry));
  }

  void set_current_entry_index(int i) { current_entry_index_ = i; }

  void SetWindowId(SessionID::id_type window_id) {
    window_id_.set_id(window_id);
  }

  void SetSessionId(SessionID::id_type id) { tab_id_.set_id(id); }

  void set_blocked_navigations(
      const std::vector<std::unique_ptr<sessions::SerializedNavigationEntry>>&
          navs) {
    for (auto& entry : navs) {
      blocked_navigations_.push_back(
          base::MakeUnique<sessions::SerializedNavigationEntry>(*entry));
    }
  }

  void reset() {
    current_entry_index_ = 0;
    sync_id_ = TabNodePool::kInvalidTabNodeID;
    entries_.clear();
  }

 private:
  int current_entry_index_ = -1;
  bool is_supervised_ = false;
  int sync_id_ = -1;
  SessionID tab_id_;
  SessionID window_id_;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      entries_;
};

// A placeholder delegate. These delegates have no WebContents, simulating a tab
// that has been restored without bringing its state fully into memory (for
// example on Android), or where the tab's contents have been evicted from
// memory. See SyncedTabDelegate::IsPlaceHolderTab for more info.
class PlaceholderTabDelegate : public SyncedTabDelegate {
 public:
  PlaceholderTabDelegate(SessionID::id_type session_id, int sync_id)
      : session_id_(session_id), sync_id_(sync_id) {}
  ~PlaceholderTabDelegate() override {}

  // SyncedTabDelegate overrides.
  SessionID::id_type GetSessionId() const override { return session_id_; }
  int GetSyncId() const override { return sync_id_; }
  void SetSyncId(int sync_id) override { sync_id_ = sync_id; }
  bool IsPlaceholderTab() const override { return true; }

  // Everything else is invalid to invoke as it depends on a valid WebContents.
  SessionID::id_type GetWindowId() const override {
    NOTREACHED();
    return 0;
  }
  bool IsBeingDestroyed() const override {
    NOTREACHED();
    return false;
  }
  std::string GetExtensionAppId() const override {
    NOTREACHED();
    return "";
  }
  bool IsInitialBlankNavigation() const override {
    NOTREACHED();
    return false;
  }
  int GetCurrentEntryIndex() const override {
    NOTREACHED();
    return 0;
  }
  int GetEntryCount() const override {
    NOTREACHED();
    return 0;
  }
  GURL GetVirtualURLAtIndex(int i) const override {
    NOTREACHED();
    return GURL();
  }
  GURL GetFaviconURLAtIndex(int i) const override {
    NOTREACHED();
    return GURL();
  }
  ui::PageTransition GetTransitionAtIndex(int i) const override {
    NOTREACHED();
    return ui::PageTransition();
  }
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override {
    NOTREACHED();
  }
  bool ProfileIsSupervised() const override {
    NOTREACHED();
    return false;
  }
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override {
    NOTREACHED();
    return nullptr;
  }
  bool ShouldSync(SyncSessionsClient* sessions_client) override {
    NOTREACHED();
    return false;
  }

 private:
  SessionID::id_type session_id_;
  int sync_id_;
};

class TestSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  TestSyncedWindowDelegate() {}
  ~TestSyncedWindowDelegate() override {}

  bool HasWindow() const override { return true; }

  SessionID::id_type GetSessionId() const override { return window_id_.id(); }

  int GetTabCount() const override { return tab_delegates_.size(); }

  int GetActiveIndex() const override { return 0; }

  bool IsApp() const override { return false; }

  bool IsTypeTabbed() const override {
    return window_type_ == sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
  }

  bool IsTypePopup() const override {
    return window_type_ == sync_pb::SessionWindow_BrowserType_TYPE_POPUP;
  }

  bool IsTabPinned(const SyncedTabDelegate* tab) const override {
    return false;
  }

  SyncedTabDelegate* GetTabAt(int index) const override {
    if (tab_delegates_.find(index) != tab_delegates_.end())
      return tab_delegates_.find(index)->second;

    return nullptr;
  }

  void OverrideWindowId(SessionID::id_type id) { window_id_.set_id(id); }

  void OverrideTabAt(int index, SyncedTabDelegate* delegate) {
    tab_delegates_[index] = delegate;
  }

  SessionID::id_type GetTabIdAt(int index) const override {
    SyncedTabDelegate* delegate = GetTabAt(index);
    if (!delegate)
      return TabNodePool::kInvalidTabID;
    return delegate->GetSessionId();
  }

  bool IsSessionRestoreInProgress() const override { return false; }

  bool ShouldSync() const override { return true; }

 private:
  std::map<int, SyncedTabDelegate*> tab_delegates_;
  SessionID window_id_;
  sync_pb::SessionWindow_BrowserType window_type_ =
      sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
};

class TestSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter() {}
  ~TestSyncedWindowDelegatesGetter() override {}

  SyncedWindowDelegateMap GetSyncedWindowDelegates() override {
    return delegates_;
  }

  const SyncedWindowDelegate* FindById(SessionID::id_type id) override {
    for (auto window_iter_pair : delegates_) {
      if (window_iter_pair.second->GetSessionId() == id)
        return window_iter_pair.second;
    }
    return nullptr;
  }

  void AddSyncedWindowDelegate(const SyncedWindowDelegate* delegate) {
    delegates_[delegate->GetSessionId()] = delegate;
  }

 private:
  SyncedWindowDelegateMap delegates_;
};

class TestSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncChangeProcessor(SyncChangeList* output) : output_(output) {}
  SyncError ProcessSyncChanges(const tracked_objects::Location& from_here,
                               const SyncChangeList& change_list) override {
    if (error_.IsSet()) {
      SyncError error = error_;
      error_ = SyncError();
      return error;
    }

    if (output_)
      output_->insert(output_->end(), change_list.begin(), change_list.end());
    NotifyLocalChangeObservers();

    return SyncError();
  }

  SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return sync_data_to_return_;
  }

  void AddLocalChangeObserver(syncer::LocalChangeObserver* observer) override {
    local_change_observers_.AddObserver(observer);
  }
  void RemoveLocalChangeObserver(
      syncer::LocalChangeObserver* observer) override {
    local_change_observers_.RemoveObserver(observer);
  }

  void NotifyLocalChangeObservers() {
    const SyncChange empty_change;
    for (syncer::LocalChangeObserver& observer : local_change_observers_)
      observer.OnLocalChange(nullptr, empty_change);
  }

  void FailProcessSyncChangesWith(const SyncError& error) { error_ = error; }

  void SetSyncDataToReturn(const SyncDataList& data) {
    sync_data_to_return_ = data;
  }

 private:
  SyncError error_;
  SyncChangeList* output_;
  SyncDataList sync_data_to_return_;
  base::ObserverList<syncer::LocalChangeObserver> local_change_observers_;
};

class DummyRouter : public LocalSessionEventRouter {
 public:
  ~DummyRouter() override {}
  void StartRoutingTo(LocalSessionEventHandler* handler) override {
    handler_ = handler;
  }
  void Stop() override {}

  void NotifyNav(SyncedTabDelegate* tab) {
    if (handler_)
      handler_->OnLocalTabModified(tab);
  }

 private:
  LocalSessionEventHandler* handler_ = nullptr;
};

// Provides ability to override SyncedWindowDelegatesGetter.
// All other calls are passed through to the original FakeSyncSessionsClient.
class SyncSessionsClientShim : public FakeSyncSessionsClient {
 public:
  explicit SyncSessionsClientShim(
      SyncedWindowDelegatesGetter* synced_window_getter)
      : synced_window_getter_(synced_window_getter) {}
  ~SyncSessionsClientShim() override {}

  SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter() override {
    // The idea here is to allow the test code override the default
    // SyncedWindowDelegatesGetter provided by |sync_sessions_client_|.
    // If |synced_window_getter_| is explicitly set, return it; otherwise return
    // the default one provided by |sync_sessions_client_|.
    return synced_window_getter_;
  }

 private:
  SyncedWindowDelegatesGetter* synced_window_getter_;
};

}  // namespace

class SessionsSyncManagerTest : public testing::Test {
 protected:
  SessionsSyncManagerTest() {}

  void SetUp() override {
    local_device_ = base::MakeUnique<LocalDeviceInfoProviderMock>(
        "cache_guid", "Wayne Gretzky's Hacking Box", "Chromium 10k",
        "Chrome 10k", sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id");
    sync_client_ = base::MakeUnique<syncer::FakeSyncClient>();
    sessions_client_shim_ =
        base::MakeUnique<SyncSessionsClientShim>(&window_getter_);
    sync_prefs_ =
        base::MakeUnique<syncer::SyncPrefs>(sync_client_->GetPrefService());
    manager_ = base::MakeUnique<SessionsSyncManager>(
        sessions_client_shim(), sync_prefs_.get(), local_device_.get(),
        std::unique_ptr<LocalSessionEventRouter>(NewDummyRouter()),
        base::Bind(&SessionNotificationObserver::NotifyOfUpdate,
                   base::Unretained(&observer_)),
        base::Bind(&SessionNotificationObserver::NotifyOfRefresh,
                   base::Unretained(&observer_)));
  }

  void TearDown() override {
    test_processor_ = nullptr;
    helper()->Reset();
    sync_prefs_.reset();
    manager_.reset();
  }

  const DeviceInfo* GetLocalDeviceInfo() {
    return local_device_->GetLocalDeviceInfo();
  }

  TabNodePool* GetTabPool() {
    return &manager()->session_tracker_.local_tab_pool_;
  }

  SessionsSyncManager* manager() { return manager_.get(); }
  SessionSyncTestHelper* helper() { return &helper_; }
  LocalDeviceInfoProvider* local_device() { return local_device_.get(); }
  SessionNotificationObserver* observer() { return &observer_; }
  syncer::SyncPrefs* sync_prefs() { return sync_prefs_.get(); }
  SyncSessionsClient* sessions_client_shim() {
    return sessions_client_shim_.get();
  }
  SyncedWindowDelegatesGetter* window_getter() { return &window_getter_; }

  std::unique_ptr<LocalSessionEventRouter> NewDummyRouter() {
    std::unique_ptr<DummyRouter> router(new DummyRouter());
    router_ = router.get();
    return std::unique_ptr<LocalSessionEventRouter>(std::move(router));
  }

  void InitWithSyncDataTakeOutput(const SyncDataList& initial_data,
                                  SyncChangeList* output) {
    test_processor_ = new TestSyncChangeProcessor(output);
    syncer::SyncMergeResult result = manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, initial_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(test_processor_),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(SyncDataList(), nullptr);
  }

  void TriggerProcessSyncChangesError() {
    test_processor_->FailProcessSyncChangesWith(SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR, "Error", syncer::SESSIONS));
  }

  void SetSyncData(const SyncDataList& data) {
    test_processor_->SetSyncDataToReturn(data);
  }

  void VerifyLocalHeaderChange(const SyncChange& change,
                               int num_windows,
                               int num_tabs) {
    SCOPED_TRACE(change.ToString());
    SyncDataLocal data(change.sync_data());
    EXPECT_EQ(manager()->current_machine_tag(), data.GetTag());
    ASSERT_TRUE(data.GetSpecifics().session().has_header());
    EXPECT_FALSE(data.GetSpecifics().session().has_tab());
    EXPECT_TRUE(data.GetSpecifics().session().header().has_device_type());
    EXPECT_EQ(GetLocalDeviceInfo()->client_name(),
              data.GetSpecifics().session().header().client_name());
    EXPECT_EQ(num_windows,
              data.GetSpecifics().session().header().window_size());
    int tab_count = 0;
    for (auto& window : data.GetSpecifics().session().header().window()) {
      tab_count += window.tab_size();
    }
    EXPECT_EQ(num_tabs, tab_count);
  }

  void VerifyLocalTabChange(const SyncChange& change,
                            int num_navigations,
                            std::string final_url) {
    SCOPED_TRACE(change.ToString());
    SyncDataLocal data(change.sync_data());
    EXPECT_TRUE(base::StartsWith(data.GetTag(),
                                 manager()->current_machine_tag(),
                                 base::CompareCase::SENSITIVE));
    EXPECT_FALSE(data.GetSpecifics().session().has_header());
    ASSERT_TRUE(data.GetSpecifics().session().has_tab());
    ASSERT_EQ(num_navigations,
              data.GetSpecifics().session().tab().navigation_size());
    EXPECT_EQ(final_url, data.GetSpecifics()
                             .session()
                             .tab()
                             .navigation(num_navigations - 1)
                             .virtual_url());
  }

  SyncChangeList* FilterOutLocalHeaderChanges(SyncChangeList* list) {
    SyncChangeList::iterator it = list->begin();
    bool found = false;
    while (it != list->end()) {
      if (it->sync_data().IsLocal() &&
          SyncDataLocal(it->sync_data()).GetTag() ==
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

  SyncChange MakeRemoteChange(const sync_pb::SessionSpecifics& specifics,
                              SyncChange::SyncChangeType type) const {
    return SyncChange(FROM_HERE, type, CreateRemoteData(specifics));
  }

  void AddTabsToChangeList(const std::vector<sync_pb::SessionSpecifics>& batch,
                           SyncChange::SyncChangeType type,
                           SyncChangeList* change_list) const {
    for (const auto& specifics : batch) {
      change_list->push_back(
          SyncChange(FROM_HERE, type, CreateRemoteData(specifics)));
    }
  }

  void AddToSyncDataList(const sync_pb::SessionSpecifics& specifics,
                         SyncDataList* list,
                         base::Time mtime) const {
    list->push_back(CreateRemoteData(specifics, mtime));
  }

  void AddTabsToSyncDataList(const std::vector<sync_pb::SessionSpecifics>& tabs,
                             SyncDataList* list) const {
    for (size_t i = 0; i < tabs.size(); ++i) {
      AddToSyncDataList(tabs[i], list, base::Time::FromInternalValue(i + 1));
    }
  }

  SyncData CreateRemoteData(const sync_pb::SessionSpecifics& specifics,
                            base::Time mtime = base::Time()) const {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(specifics);
    return CreateRemoteData(entity, mtime);
  }

  SyncData CreateRemoteData(const sync_pb::EntitySpecifics& entity,
                            base::Time mtime = base::Time()) const {
    // The server ID is never relevant to these tests, so just use 1.
    return SyncData::CreateRemoteData(
        1, entity, mtime, syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create(),
        SessionsSyncManager::TagHashFromSpecifics(entity.session()));
  }

  // Creates a new tab within the window specified by |window_id|, and points it
  // at |url|. Returns the newly created TestSyncedTabDelegate (not owned).
  TestSyncedTabDelegate* AddTab(SessionID::id_type window_id,
                                const std::string& url,
                                base::Time time) {
    tabs_.push_back(base::MakeUnique<TestSyncedTabDelegate>());
    for (auto& window : windows_) {
      if (window->GetSessionId() == window_id) {
        int tab_index = window->GetTabCount();
        window->OverrideTabAt(tab_index, tabs_.back().get());
      }
    }

    // Simulate the browser firing a tab parented notification, ahead of the
    // actual navigation.
    router_->NotifyNav(tabs_.back().get());

    // Now do the actual navigation.
    NavigateTab(tabs_.back().get(), url, time);
    return tabs_.back().get();
  }
  TestSyncedTabDelegate* AddTab(SessionID::id_type window_id,
                                const std::string& url) {
    return AddTab(window_id, url, base::Time());
  }

  void NavigateTab(TestSyncedTabDelegate* delegate,
                   const std::string& url,
                   base::Time time) {
    auto entry = base::MakeUnique<sessions::SerializedNavigationEntry>(
        SerializedNavigationEntryTestHelper::CreateNavigation(url, kTitle));
    SerializedNavigationEntryTestHelper::SetTimestamp(time, entry.get());
    delegate->AppendEntry(std::move(entry));
    delegate->set_current_entry_index(delegate->GetCurrentEntryIndex() + 1);
    router_->NotifyNav(delegate);
  }
  void NavigateTab(TestSyncedTabDelegate* delegate, const std::string& url) {
    NavigateTab(delegate, url, base::Time());
  }

  TestSyncedWindowDelegate* AddWindow() {
    windows_.push_back(base::MakeUnique<TestSyncedWindowDelegate>());
    window_getter_.AddSyncedWindowDelegate(windows_.back().get());
    return windows_.back().get();
  }

 private:
  std::unique_ptr<syncer::FakeSyncClient> sync_client_;
  std::unique_ptr<SyncSessionsClientShim> sessions_client_shim_;
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;
  SessionNotificationObserver observer_;
  DummyRouter* router_ = nullptr;
  std::unique_ptr<SessionsSyncManager> manager_;
  SessionSyncTestHelper helper_;
  TestSyncChangeProcessor* test_processor_ = nullptr;
  TestSyncedWindowDelegatesGetter window_getter_;
  std::vector<std::unique_ptr<TestSyncedWindowDelegate>> windows_;
  std::vector<std::unique_ptr<TestSyncedTabDelegate>> tabs_;
  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;
};

// Test that the SyncSessionManager can properly fill in a SessionHeader.
TEST_F(SessionsSyncManagerTest, PopulateSessionHeader) {
  sync_pb::SessionHeader header_s;
  header_s.set_client_name("Client 1");
  header_s.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);

  SyncedSession session;
  base::Time time = base::Time::Now();
  SessionsSyncManager::PopulateSessionHeaderFromSpecifics(header_s, time,
                                                          &session);
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

  SyncedSession* session = manager()->session_tracker_.GetSession(kTag1);
  manager()->session_tracker_.PutWindowInSession(kTag1, 0);
  manager()->BuildSyncedSessionFromSpecifics(kTag1, window_s, base::Time(),
                                             session->windows[0].get());
  ASSERT_EQ(1U, session->windows[0]->tabs.size());
  ASSERT_EQ(1, session->windows[0]->selected_tab_index);
  ASSERT_EQ(sessions::SessionWindow::TYPE_TABBED, session->windows[0]->type);
  ASSERT_EQ(1U, manager()->session_tracker_.num_synced_sessions());
  ASSERT_EQ(1U, manager()->session_tracker_.num_synced_tabs(kTag1));
}

// Populate the fake tab delegate with some data and navigation
// entries and make sure that setting a SessionTab from it preserves
// those entries (and clobbers any existing data).
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegate) {
  // Create a tab with three valid entries.
  TestSyncedTabDelegate* tab =
      AddTab(AddWindow()->GetSessionId(), kFoo1, kTime1);
  NavigateTab(tab, kBar1, kTime2);
  NavigateTab(tab, kBaz1, kTime3);

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
  manager()->SetSessionTabFromDelegate(*tab, kTime4, &session_tab);

  EXPECT_EQ(tab->GetWindowId(), session_tab.window_id.id());
  EXPECT_EQ(tab->GetSessionId(), session_tab.tab_id.id());
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
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegateNavigationIndex) {
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  const int kNavs = 10;
  for (int i = 1; i < kNavs; ++i) {
    NavigateTab(tab, base::StringPrintf("http://foo%i", i));
  }
  tab->set_current_entry_index(kNavs - 2);

  sessions::SessionTab session_tab;
  manager()->SetSessionTabFromDelegate(*tab, kTime9, &session_tab);

  EXPECT_EQ(6, session_tab.current_navigation_index);
  ASSERT_EQ(8u, session_tab.navigations.size());
  EXPECT_EQ(GURL("http://foo2"), session_tab.navigations[0].virtual_url());
  EXPECT_EQ(GURL("http://foo3"), session_tab.navigations[1].virtual_url());
  EXPECT_EQ(GURL("http://foo4"), session_tab.navigations[2].virtual_url());
}

// Ensure the current_navigation_index gets set to the end of the navigation
// stack if the current navigation is invalid.
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegateCurrentInvalid) {
  TestSyncedTabDelegate* tab =
      AddTab(AddWindow()->GetSessionId(), kFoo1, kTime0);
  NavigateTab(tab, std::string(""), kTime1);
  NavigateTab(tab, kBar1, kTime2);
  NavigateTab(tab, kBar2, kTime3);
  tab->set_current_entry_index(1);

  sessions::SessionTab session_tab;
  manager()->SetSessionTabFromDelegate(*tab, kTime9, &session_tab);

  EXPECT_EQ(2, session_tab.current_navigation_index);
  ASSERT_EQ(3u, session_tab.navigations.size());
}

// Tests that for supervised users blocked navigations are recorded and marked
// as such, while regular navigations are marked as allowed.
TEST_F(SessionsSyncManagerTest, BlockedNavigations) {
  TestSyncedTabDelegate* tab =
      AddTab(AddWindow()->GetSessionId(), kFoo1, kTime1);

  auto entry2 = base::MakeUnique<sessions::SerializedNavigationEntry>();
  GURL url2("http://blocked.com/foo");
  SerializedNavigationEntryTestHelper::SetVirtualURL(GURL(url2), entry2.get());
  SerializedNavigationEntryTestHelper::SetTimestamp(kTime2, entry2.get());

  auto entry3 = base::MakeUnique<sessions::SerializedNavigationEntry>();
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
  manager()->SetSessionTabFromDelegate(*tab, kTime4, &session_tab);

  EXPECT_EQ(tab->GetWindowId(), session_tab.window_id.id());
  EXPECT_EQ(tab->GetSessionId(), session_tab.tab_id.id());
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

// Tests that the local session header objects is created properly in
// presence of no other session activity, once and only once.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionNoTabs) {
  // Add a single window with no tabs.
  AddWindow();

  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  EXPECT_FALSE(manager()->current_machine_tag().empty());

  // Header creation + update.
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));
  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalHeaderChange(out[1], 0, 0);

  // Now take that header node and feed it in as input.
  SyncData d = CreateRemoteData(out[1].sync_data().GetSpecifics());
  SyncDataList in = {d};
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput(in, &out);

  ASSERT_TRUE(ChangeTypeMatches(out, {SyncChange::ACTION_UPDATE}));
  EXPECT_TRUE(out[0].sync_data().GetSpecifics().session().has_header());
}

// Tests MergeDataAndStartSyncing with sync data but no local data.
TEST_F(SessionsSyncManagerTest, MergeWithInitialForeignSession) {
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));
  // Add a second window.
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &initial_data);
  for (auto tab_id : tab_list2) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, 0, tab_id, entity.mutable_session());
    initial_data.push_back(CreateRemoteData(entity));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
  EXPECT_TRUE(FilterOutLocalHeaderChanges(&output)->empty());

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list1);
  session_reference.push_back(tab_list2);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionExistingTabs) {
  TestSyncedWindowDelegate* window = AddWindow();
  SessionID::id_type window_id = window->GetSessionId();
  TestSyncedTabDelegate* tab = AddTab(window_id, kFoo1);
  NavigateTab(tab, kBar1);  // Adds back entry.
  NavigateTab(tab, kBaz1);  // Adds back entry.
  TestSyncedTabDelegate* tab2 = AddTab(window_id, kFoo2);
  NavigateTab(tab2, kBar2);  // Adds back entry.

  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  // Header creation, add two tabs, header update.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));

  // Check that this machine's data is not included in the foreign windows.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));

  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalTabChange(out[1], tab->GetEntryCount(), kBaz1);
  VerifyLocalTabChange(out[2], tab2->GetEntryCount(), kBar2);
  VerifyLocalHeaderChange(out[3], 1, 2);

  // Verify tab delegates have Sync ids.
  EXPECT_EQ(0, window->GetTabAt(0)->GetSyncId());
  EXPECT_EQ(1, window->GetTabAt(1)->GetSyncId());
}

// This is a combination of MergeWithInitialForeignSession and
// MergeLocalSessionExistingTabs. We repeat some checks performed in each of
// those tests to ensure the common mixed scenario works.
TEST_F(SessionsSyncManagerTest, MergeWithLocalAndForeignTabs) {
  // Local.
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  NavigateTab(tab, kFoo2);

  // Foreign.
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));
  SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &foreign_data);

  SyncChangeList out;
  InitWithSyncDataTakeOutput(foreign_data, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_TRUE(ChangeTypeMatches(out,
                                {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                                 SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));

  // Verify local data.
  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalTabChange(out[1], tab->GetEntryCount(), kFoo2);
  VerifyLocalHeaderChange(out[2], 1, 1);

  // Verify foreign data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
  // There should be one and only one foreign session. If VerifySyncedSession
  // was successful above this EXPECT call ensures the local session didn't
  // get mistakenly added to foreign tracking (Similar to ExistingTabs test).
  EXPECT_EQ(1U, foreign_sessions.size());
}

// Tests the common scenario.  Merge with both local and foreign session data
// followed by updates flowing from sync and local.
TEST_F(SessionsSyncManagerTest, UpdatesAfterMixedMerge) {
  // Add local and foreign data.
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  NavigateTab(tab, kFoo2);
  AddTab(AddWindow()->GetSessionId(), kBar1);

  SyncDataList foreign_data1;
  std::vector<std::vector<SessionID::id_type>> meta1_reference;
  sync_pb::SessionSpecifics meta1;

  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  meta1_reference.push_back(tab_list1);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  meta1 = helper()->BuildForeignSession(kTag1, tab_list1, &tabs1);
  foreign_data1.push_back(CreateRemoteData(meta1));
  AddTabsToSyncDataList(tabs1, &foreign_data1);

  SyncChangeList out;
  InitWithSyncDataTakeOutput(foreign_data1, &out);

  // 1 header add, two tab adds, one header update.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));
  VerifyLocalHeaderChange(out[3], 2, 2);

  // Add a second window to the foreign session.
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  meta1_reference.push_back(tab_list2);
  helper()->AddWindowSpecifics(1, tab_list2, &meta1);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(tab_list2.size());
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    helper()->BuildTabSpecifics(kTag1, 0, tab_list2[i], &tabs2[i]);
  }

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs2, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(1)->second->tabs.size());
  helper()->VerifySyncedSession(kTag1, meta1_reference, *(foreign_sessions[0]));

  // Add a new foreign session.
  std::vector<SessionID::id_type> tag2_tab_list = {107, 115};
  std::vector<sync_pb::SessionSpecifics> tag2_tabs;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, tag2_tab_list, &tag2_tabs));
  changes.push_back(MakeRemoteChange(meta2, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tag2_tabs, SyncChange::ACTION_ADD, &changes);

  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type>> meta2_reference;
  meta2_reference.push_back(tag2_tab_list);
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[1]->windows.find(0)->second->tabs.size());
  helper()->VerifySyncedSession(kTag2, meta2_reference, *(foreign_sessions[1]));
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
  SyncChangeList removal;
  removal.push_back(MakeRemoteChange(meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_UPDATE, &removal);
  manager()->ProcessSyncChanges(FROM_HERE, removal);

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(3U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  helper()->VerifySyncedSession(kTag1, meta1_reference, *(foreign_sessions[0]));
}

// Tests that this SyncSessionManager knows how to delete foreign sessions
// if it wants to.
TEST_F(SessionsSyncManagerTest, DeleteForeignSession) {
  InitWithNoSyncData();
  SyncChangeList changes;

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  manager()->DeleteForeignSessionInternal(kTag1, &changes);
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  EXPECT_TRUE(changes.empty());

  // Fill an instance of session specifics with a foreign session's data.
  std::vector<sync_pb::SessionSpecifics> tabs;
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs));

  // Update associator with the session's meta node, window, and tabs.
  manager()->UpdateTrackerWithSpecifics(meta, base::Time());
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs.begin();
       iter != tabs.end(); ++iter) {
    manager()->UpdateTrackerWithSpecifics(*iter, base::Time());
  }
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  // Now delete the foreign session.
  manager()->DeleteForeignSessionInternal(kTag1, &changes);
  EXPECT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));

  EXPECT_EQ(5U, changes.size());
  ASSERT_TRUE(AllOfChangesAreType(changes, SyncChange::ACTION_DELETE));
  std::set<std::string> expected_tags(&kTag1, &kTag1 + 1);
  for (int i = 0; i < 5; ++i)
    expected_tags.insert(TabNodeIdToTag(kTag1, i));

  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(changes[i].ToString());
    EXPECT_TRUE(changes[i].IsValid());
    EXPECT_TRUE(changes[i].sync_data().IsValid());
    EXPECT_EQ(1U, expected_tags.erase(
                      SyncDataLocal(changes[i].sync_data()).GetTag()));
  }
}

// Write a foreign session to a node, with the tabs arriving first, and then
// retrieve it.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeTabsFirst) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));

  SyncChangeList adds;
  // Add tabs for first window, then the meta node.
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &adds);
  adds.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, adds);

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session to a node with some tabs that never arrive.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeMissingTabs) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));
  // Add a second window, but this time only create two tab nodes, despite the
  // window expecting four tabs.
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i], &tabs2[i]);
  }

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
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
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_UPDATE));
  // Update associator with the session's meta node containing one window.
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  // Check that the foreign session was associated and retrieve the data.
  foreign_sessions.clear();
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows.size());
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Tests that the SessionsSyncManager can handle a remote client deleting
// sync nodes that belong to this local session.
TEST_F(SessionsSyncManagerTest, ProcessRemoteDeleteOfLocalSession) {
  SessionID::id_type window_id = AddWindow()->GetSessionId();
  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_EQ(2U, out.size());

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(
      out[1].sync_data().GetSpecifics().session(), SyncChange::ACTION_DELETE));
  out.clear();
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(manager()->local_tab_pool_out_of_sync_);
  EXPECT_TRUE(out.empty());  // ChangeProcessor shouldn't see any activity.

  // This should trigger repair of the TabNodePool.
  AddTab(window_id, kFoo1);
  EXPECT_FALSE(manager()->local_tab_pool_out_of_sync_);

  // Rebuilding associations will trigger an initial header add and update,
  // coupled with the tab creation and the header update to reflect the new tab.
  // In total, that means four changes.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));

  // Verify the actual content.
  VerifyLocalTabChange(out[2], 1, kFoo1);
  VerifyLocalHeaderChange(out[3], 1, 1);

  // Verify TabLinks.
  EXPECT_EQ(1U, GetTabPool()->Capacity());
  EXPECT_TRUE(GetTabPool()->Empty());
  int tab_node_id = out[2].sync_data().GetSpecifics().session().tab_node_id();
  int tab_id = out[2].sync_data().GetSpecifics().session().tab().tab_id();
  EXPECT_EQ(tab_id, GetTabPool()->GetTabIdFromTabNodeId(tab_node_id));
}

// Test that receiving a session delete from sync removes the session
// from tracking.
TEST_F(SessionsSyncManagerTest, ProcessForeignDelete) {
  InitWithNoSyncData();
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession("tag1", tab_list1, &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  changes.clear();
  foreign_sessions.clear();
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabs) {
  SyncDataList foreign_data;
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
  std::vector<SessionID::id_type> tab_list = {0, 1, 2, 3, 4, 5};
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

  AddWindow();
  SyncChangeList output;
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

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tabs[2], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tabs[4], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(orphan6, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_UPDATE));
  changes.push_back(MakeRemoteChange(tabs[3], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tabs[5], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(orphan7, SyncChange::ACTION_DELETE));
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
  SyncDataList foreign_data;
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

  AddWindow();
  SyncChangeList output;
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
  manager()->session_tracker_.LookupForeignTabNodeIds(session_tag,
                                                      &tab_node_ids);
  EXPECT_EQ(6U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab1A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab1B.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab1C.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2B.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2C.tab_node_id()) != tab_node_ids.end());

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tab1A, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tab1B, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tab2C, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  tab_node_ids.clear();
  manager()->session_tracker_.LookupForeignTabNodeIds(session_tag,
                                                      &tab_node_ids);
  EXPECT_EQ(3U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab1C.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2A.tab_node_id()) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab2B.tab_node_id()) != tab_node_ids.end());

  manager()->DoGarbageCollection();
  ASSERT_EQ(3U, output.size());
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabsWithReusedNodeIds) {
  SyncDataList foreign_data;
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

  AddWindow();
  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  std::set<int> tab_node_ids;
  manager()->session_tracker_.LookupForeignTabNodeIds(session_tag,
                                                      &tab_node_ids);
  EXPECT_EQ(2U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_shared) != tab_node_ids.end());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_unique) != tab_node_ids.end());

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tab1A, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  tab_node_ids.clear();
  manager()->session_tracker_.LookupForeignTabNodeIds(session_tag,
                                                      &tab_node_ids);
  EXPECT_EQ(1U, tab_node_ids.size());
  EXPECT_TRUE(tab_node_ids.find(tab_node_id_unique) != tab_node_ids.end());

  manager()->DoGarbageCollection();
  EXPECT_EQ(1U, output.size());
}

TEST_F(SessionsSyncManagerTest, AssociationReusesNodes) {
  SyncChangeList changes;
  AddTab(AddWindow()->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(SyncDataList(), &changes);
  ASSERT_TRUE(ChangeTypeMatches(changes,
                                {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                                 SyncChange::ACTION_UPDATE}));
  ASSERT_TRUE(changes[1].sync_data().GetSpecifics().session().has_tab());
  int tab_node_id =
      changes[1].sync_data().GetSpecifics().session().tab_node_id();

  // Pass back the previous tab and header nodes at association, along with a
  // second tab node (with a rewritten tab node id).
  SyncDataList in;
  in.push_back(
      CreateRemoteData(changes[2].sync_data().GetSpecifics()));  // Header node.
  sync_pb::SessionSpecifics new_tab(
      changes[1].sync_data().GetSpecifics().session());
  new_tab.set_tab_node_id(tab_node_id + 1);
  in.push_back(CreateRemoteData(new_tab));  // New tab node.
  in.push_back(CreateRemoteData(
      changes[1].sync_data().GetSpecifics()));  // Old tab node.
  changes.clear();

  // Reassociate (with the same single tab/window open).
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput(in, &changes);

  // No tab entities should be deleted. The original (lower) tab node id should
  // be reused for association.
  FilterOutLocalHeaderChanges(&changes);
  ASSERT_TRUE(ChangeTypeMatches(changes, {SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(changes[0], 1, kFoo1);
  EXPECT_EQ(tab_node_id,
            changes[0].sync_data().GetSpecifics().session().tab_node_id());
}

// Ensure that the merge process deletes a tab node without a tab id.
TEST_F(SessionsSyncManagerTest, MergeDeletesTabMissingTabId) {
  SyncChangeList changes;
  InitWithNoSyncData();

  std::string local_tag = manager()->current_machine_tag();
  int tab_node_id = 0;
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(local_tag);
  specifics.set_tab_node_id(tab_node_id);
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput({CreateRemoteData(specifics)}, &changes);
  EXPECT_EQ(1U, FilterOutLocalHeaderChanges(&changes)->size());
  EXPECT_EQ(SyncChange::ACTION_DELETE, changes[0].change_type());
  EXPECT_EQ(TabNodeIdToTag(local_tag, tab_node_id),
            SyncDataLocal(changes[0].sync_data()).GetTag());
}

// Verifies that we drop both headers and tabs during merge if their stored tag
// hash doesn't match a computer tag hash. This mitigates potential failures
// while cleaning up bad foreign data, see crbug.com/604657.
TEST_F(SessionsSyncManagerTest, MergeDeletesBadHash) {
  SyncDataList foreign_data;
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

  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, FilterOutLocalHeaderChanges(&output)->size());
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
  EXPECT_EQ(1U, CountIfTagMatches(output, bad_header_tag));
  EXPECT_EQ(1U, CountIfTagMatches(output, bad_tab_tag));

  std::vector<const SyncedSession*> sessions;
  manager()->session_tracker_.LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW);
  ASSERT_EQ(2U, sessions.size());
  EXPECT_EQ(1U, CountIfTagMatches(sessions, good_header_tag));
  EXPECT_EQ(1U, CountIfTagMatches(sessions, good_tag_tab));
}

// Test that things work if a tab is initially ignored.
TEST_F(SessionsSyncManagerTest, AssociateWindowsDontReloadTabs) {
  SyncChangeList out;
  // Go to a URL that is ignored by session syncing.
  TestSyncedTabDelegate* tab =
      AddTab(AddWindow()->GetSessionId(), "chrome://preferences/");
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  VerifyLocalHeaderChange(out[1], 0, 0);
  out.clear();

  // Go to a sync-interesting URL.
  NavigateTab(tab, kFoo1);

  // The tab should be created, coupled with a header update.
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(out[0], 2, kFoo1);
  VerifyLocalHeaderChange(out[1], 1, 1);
}

// Tests that the SyncSessionManager responds to local tab events properly.
TEST_F(SessionsSyncManagerTest, OnLocalTabModified) {
  SyncChangeList out;
  // Init with no local data, relies on MergeLocalSessionNoTabs.
  TestSyncedWindowDelegate* window = AddWindow();
  SessionID::id_type window_id = window->GetSessionId();
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_FALSE(manager()->current_machine_tag().empty());
  ASSERT_EQ(2U, out.size());

  // Copy the original header.
  sync_pb::EntitySpecifics header(out[0].sync_data().GetSpecifics());
  out.clear();

  NavigateTab(AddTab(window_id, kFoo1), kFoo2);
  NavigateTab(AddTab(window_id, kBar1), kBar2);
  std::vector<std::string> urls = {kFoo1, kFoo2, kBar1, kBar2};

  // Change type breakdown:
  // 1 tab add + 2 header updates.
  const size_t kChangesPerTabCreation = 3;
  // 1 tab update + 1 header update.
  const size_t kChangesPerTabNav = 2;
  const size_t kChangesPerTab = kChangesPerTabNav + kChangesPerTabCreation;
  const size_t kNumTabs = 2;
  const size_t kTotalUpdates = kChangesPerTab * kNumTabs;

  std::vector<SyncChange::SyncChangeType> types = {
      // Tab 1
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_ADD,
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE,
      SyncChange::ACTION_UPDATE,
      // Tab 2
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_ADD,
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE,
      SyncChange::ACTION_UPDATE};
  ASSERT_EQ(kTotalUpdates, types.size());

  // Verify the tab node creations and updates to ensure the SyncProcessor sees
  // the right operations. Do this by inspecting the set of changes for each
  // tab separately by iterating through the tabs.
  ASSERT_TRUE(ChangeTypeMatches(out, types));
  for (size_t i = 0; i < kNumTabs; ++i) {
    int index = kChangesPerTab * i;
    int nav_per_tab_count = 0;
    {
      SCOPED_TRACE(index);
      // The initial tab parent event triggers a header update (which is in
      // effect a no-op).
      VerifyLocalHeaderChange(out[index++], (i == 0 ? 0 : 1), i);
    }
    {
      SCOPED_TRACE(index);
      nav_per_tab_count++;
      // Tab update after initial creation..
      VerifyLocalTabChange(out[index++], nav_per_tab_count,
                           urls[i * kChangesPerTabNav + nav_per_tab_count - 1]);
    }
    {
      SCOPED_TRACE(index);
      // The associate windows after the tab creation.
      VerifyLocalHeaderChange(out[index++], 1, i + 1);
    }
    {
      SCOPED_TRACE(index);
      nav_per_tab_count++;
      // Tab navigation.
      VerifyLocalTabChange(out[index++], nav_per_tab_count,
                           urls[i * kChangesPerTabNav + nav_per_tab_count - 1]);
    }
    {
      SCOPED_TRACE(index);
      // The associate windows after the tab navigation.
      VerifyLocalHeaderChange(out[index++], 1, i + 1);
    }
  }

  // Verify tab delegates have Sync ids.
  EXPECT_EQ(0, window->GetTabAt(0)->GetSyncId());
  EXPECT_EQ(1, window->GetTabAt(1)->GetSyncId());
}

TEST_F(SessionsSyncManagerTest, ForeignSessionModifiedTime) {
  SyncDataList foreign_data;
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

  SyncChangeList output;
  AddWindow();
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
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, tab_list2, &tabs2));
  // Set the modification time for tag1 to be 21 days ago, tag2 to 5 days ago.
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  base::Time tag2_time = base::Time::Now() - base::TimeDelta::FromDays(5);

  SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta, tag1_time));
  foreign_data.push_back(CreateRemoteData(meta2, tag2_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  AddTabsToSyncDataList(tabs2, &foreign_data);

  AddWindow();
  SyncChangeList output;
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
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
  EXPECT_EQ(kTag1, SyncDataLocal(output[0].sync_data()).GetTag());
  for (int i = 1; i < 5; ++i) {
    EXPECT_EQ(TabNodeIdToTag(kTag1, i),
              SyncDataLocal(output[i].sync_data()).GetTag());
  }

  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list2);
  helper()->VerifySyncedSession(kTag2, session_reference,
                                *(foreign_sessions[0]));
}

TEST_F(SessionsSyncManagerTest, DoGarbageCollectionOrphans) {
  SyncDataList foreign_data;
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

  SyncChangeList output;
  AddWindow();
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
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
}

// Test that an update to a previously considered "stale" session,
// prior to garbage collection, will save the session from deletion.
TEST_F(SessionsSyncManagerTest, GarbageCollectionHonoursUpdate) {
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));
  SyncDataList foreign_data;
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  foreign_data.push_back(CreateRemoteData(meta, tag1_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  SyncChangeList output;
  AddWindow();
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());

  // Update to a non-stale time.
  sync_pb::EntitySpecifics update_entity;
  update_entity.mutable_session()->CopyFrom(tabs1[0]);
  SyncChangeList changes;
  changes.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                               CreateRemoteData(tabs1[0], base::Time::Now())));
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
  std::vector<std::vector<SessionID::id_type>> session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
}

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when processing
// sync changes.
TEST_F(SessionsSyncManagerTest, NotifiedOfUpdates) {
  ASSERT_FALSE(observer()->notified_of_update());
  InitWithNoSyncData();

  SessionID::id_type n[] = {5};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession("tag1", tab_list, &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer()->notified_of_update());

  changes.clear();
  observer()->Reset();
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer()->notified_of_update());

  changes.clear();
  observer()->Reset();
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_DELETE));
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
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list, &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  observer()->Reset();
  ASSERT_FALSE(observer()->notified_of_update());
  manager()->DeleteForeignSession(tag);
  ASSERT_TRUE(observer()->notified_of_update());
}

// Tests that opening the other devices page triggers a session sync refresh.
// This page only exists on mobile platforms today; desktop has a
// search-enhanced NTP without other devices.
TEST_F(SessionsSyncManagerTest, NotifiedOfRefresh) {
  SessionID::id_type window_id = AddWindow()->GetSessionId();
  ASSERT_FALSE(observer()->notified_of_refresh());
  InitWithNoSyncData();
  TestSyncedTabDelegate* tab = AddTab(window_id, kFoo1);
  EXPECT_FALSE(observer()->notified_of_refresh());
  NavigateTab(tab, "chrome://newtab/#open_tabs");
  EXPECT_TRUE(observer()->notified_of_refresh());
}

// Tests receipt of duplicate tab IDs in the same window.  This should never
// happen, but we want to make sure the client won't do anything bad if it does
// receive such garbage input data.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInSameWindow) {
  std::string tag = "tag1";

  // Reuse tab ID 10 in an attempt to trigger bad behavior.
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, tab_list1, &tabs1));

  // Set up initial data.
  SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(CreateRemoteData(entity));
  AddTabsToSyncDataList(tabs1, &initial_data);

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of duplicate tab IDs for the same session.  The duplicate tab
// ID is present in two different windows.  A client can't be expected to do
// anything reasonable with this input, but we can expect that it doesn't
// crash.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInOtherWindow) {
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));

  // Add a second window.  Tab ID 10 is a duplicate.
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(CreateRemoteData(entity));
  AddTabsToSyncDataList(tabs1, &initial_data);

  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, 0, tab_list2[i],
                                entity.mutable_session());
    initial_data.push_back(CreateRemoteData(entity));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of multiple unassociated tabs and makes sure that
// the ones with later timestamp win
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateUnassociatedTabs) {
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  sync_pb::EntitySpecifics entity;

  for (size_t i = 0; i < tabs1.size(); ++i) {
    entity.mutable_session()->CopyFrom(tabs1[i]);
    initial_data.push_back(
        CreateRemoteData(entity, base::Time::FromDoubleT(2000)));
  }

  // Add two more tabs with duplicating IDs but with different modification
  // times, one before and one after the tabs above.
  // These two tabs get a different visual indices to distinguish them from the
  // tabs above that get visual index 1 by default.
  sync_pb::SessionSpecifics duplicating_tab1;
  helper()->BuildTabSpecifics(kTag1, 0, kTabIds1[1], &duplicating_tab1);
  duplicating_tab1.mutable_tab()->set_tab_visual_index(2);
  entity.mutable_session()->CopyFrom(duplicating_tab1);
  initial_data.push_back(
      CreateRemoteData(entity, base::Time::FromDoubleT(1000)));

  sync_pb::SessionSpecifics duplicating_tab2;
  helper()->BuildTabSpecifics(kTag1, 0, kTabIds1[2], &duplicating_tab2);
  duplicating_tab2.mutable_tab()->set_tab_visual_index(3);
  entity.mutable_session()->CopyFrom(duplicating_tab2);
  initial_data.push_back(
      CreateRemoteData(entity, base::Time::FromDoubleT(3000)));

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));

  const std::vector<std::unique_ptr<sessions::SessionTab>>& window_tabs =
      foreign_sessions[0]->windows.find(0)->second->tabs;
  ASSERT_EQ(4U, window_tabs.size());
  // The first one is from the original set of tabs.
  ASSERT_EQ(1, window_tabs[0]->tab_visual_index);
  // The one from the original set of tabs wins over duplicating_tab1.
  ASSERT_EQ(1, window_tabs[1]->tab_visual_index);
  // duplicating_tab2 wins due to the later timestamp.
  ASSERT_EQ(3, window_tabs[2]->tab_visual_index);
}

// Verify that GetAllForeignSessions returns all sessions sorted by recency.
TEST_F(SessionsSyncManagerTest, GetAllForeignSessions) {
  std::vector<SessionID::id_type> tab_list(std::begin(kTabIds1),
                                           std::end(kTabIds1));

  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta1(
      helper()->BuildForeignSession(kTag1, tab_list, &tabs1));

  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, tab_list, &tabs2));

  SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteData(meta1, base::Time::FromInternalValue(10)));
  AddTabsToSyncDataList(tabs1, &initial_data);
  initial_data.push_back(
      CreateRemoteData(meta2, base::Time::FromInternalValue(200)));
  AddTabsToSyncDataList(tabs2, &initial_data);

  SyncChangeList output;
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
  std::vector<SessionID::id_type> tab_list1(std::begin(kTabIds1),
                                            std::end(kTabIds1));
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));
  // Add a second window.
  std::vector<SessionID::id_type> tab_list2(std::begin(kTabIds2),
                                            std::end(kTabIds2));
  helper()->AddWindowSpecifics(1, tab_list2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  // Add the first window's tabs.
  AddTabsToSyncDataList(tabs1, &initial_data);

  // Add the second window's tabs.
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, 0, tab_list2[i],
                                entity.mutable_session());
    // Order the tabs oldest to most recent and left to right visually.
    initial_data.push_back(
        CreateRemoteData(entity, base::Time::FromInternalValue(i + 1)));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const sessions::SessionTab*> tabs;
  ASSERT_TRUE(manager()->GetForeignSessionTabs(kTag1, &tabs));
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

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, SwappedOutOnRestore) {
  const int kRestoredTabId = 1337;
  const int kNewTabId = 2468;

  // Start with three tabs in a window.
  TestSyncedWindowDelegate* window = AddWindow();
  TestSyncedTabDelegate* tab1 = AddTab(window->GetSessionId(), kFoo1);
  NavigateTab(tab1, kFoo2);
  TestSyncedTabDelegate* tab2 = AddTab(window->GetSessionId(), kBar1);
  NavigateTab(tab2, kBar2);
  TestSyncedTabDelegate* tab3 = AddTab(window->GetSessionId(), kBaz1);
  NavigateTab(tab3, kBaz2);

  SyncDataList in;
  SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 3 tab adds/updates, one header update.
  ASSERT_EQ(5U, out.size());

  // Now update the sync data to be:
  // * one "normal" fully loaded tab
  // * one placeholder tab with no WebContents and a tab_id change
  // * one placeholder tab with no WebContents and no tab_id change
  sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  sync_pb::EntitySpecifics t1_entity = out[2].sync_data().GetSpecifics();
  t1_entity.mutable_session()->mutable_tab()->set_tab_id(kRestoredTabId);
  sync_pb::EntitySpecifics t2_entity = out[3].sync_data().GetSpecifics();
  in.push_back(CreateRemoteData(t0_entity));
  in.push_back(CreateRemoteData(t1_entity));
  in.push_back(CreateRemoteData(t2_entity));
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);

  PlaceholderTabDelegate t1_override(kNewTabId, 1);
  PlaceholderTabDelegate t2_override(t2_entity.session().tab().tab_id(), 2);
  window->OverrideTabAt(1, &t1_override);
  window->OverrideTabAt(2, &t2_override);
  InitWithSyncDataTakeOutput(in, &out);

  // The last change should be the final header update, reflecting 1 window
  // and 3 tabs.
  VerifyLocalHeaderChange(out.back(), 1, 3);

  // There should be three changes, one for the fully associated tab, and
  // one each for the tab_id updates to t1 and t2.
  ASSERT_TRUE(AllOfChangesAreType(*FilterOutLocalHeaderChanges(&out),
                                  SyncChange::ACTION_UPDATE));
  ASSERT_EQ(3U, out.size());
  VerifyLocalTabChange(out[0], 2, kFoo2);
  VerifyLocalTabChange(out[1], 2, kBar2);
  VerifyLocalTabChange(out[2], 2, kBaz2);
}

// Ensure model association updates the window ID for tabs whose window's ID has
// changed.
TEST_F(SessionsSyncManagerTest, WindowIdUpdatedOnRestore) {
  const int kNewWindowId = 1337;
  SyncDataList in;
  SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedWindowDelegate* window = AddWindow();
  AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_EQ(3U, out.size());
  const sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  ASSERT_TRUE(t0_entity.session().has_tab());

  in.push_back(CreateRemoteData(t0_entity));
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);

  // Override the tab with a placeholder tab delegate.
  PlaceholderTabDelegate t0_override(t0_entity.session().tab().tab_id(),
                                     t0_entity.session().tab_node_id());

  // Set up the window override with the new window ID and placeholder tab.
  window->OverrideTabAt(0, &t0_override);
  window->OverrideWindowId(kNewWindowId);
  InitWithSyncDataTakeOutput(in, &out);

  // There should be one change for t0's window ID update.
  ASSERT_EQ(1U, FilterOutLocalHeaderChanges(&out)->size());
  VerifyLocalTabChange(out[0], 1, kFoo1);
  EXPECT_EQ(kNewWindowId,
            out[0].sync_data().GetSpecifics().session().tab().window_id());
}

// Ensure that the manager properly ignores a restored placeholder that refers
// to a tab node that doesn't exist
TEST_F(SessionsSyncManagerTest, RestoredPlacholderTabNodeDeleted) {
  syncer::SyncDataList in;
  syncer::SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedWindowDelegate* window = AddWindow();
  AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_EQ(3U, out.size());
  const sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  ASSERT_TRUE(t0_entity.session().has_tab());

  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);

  // Override the tab with a placeholder tab delegate.
  PlaceholderTabDelegate t0_override(t0_entity.session().tab().tab_id(),
                                     t0_entity.session().tab_node_id());

  // Override the tab with a placeholder whose sync entity won't exist.
  window->OverrideTabAt(0, &t0_override);
  InitWithSyncDataTakeOutput(in, &out);

  // Because no entities were passed in at associate time, there should be no
  // tab changes.
  ASSERT_EQ(0U, FilterOutLocalHeaderChanges(&out)->size());
}

// Check the behavior for a placeholder tab in one window being mapped to the
// same sync entity as a tab in another window. If the placeholder is associated
// last, the original tab should be unmapped from the first window, and reused
// by the placeholder in the new window..
TEST_F(SessionsSyncManagerTest, PlaceholderConflictAcrossWindows) {
  syncer::SyncDataList in;
  syncer::SyncChangeList out;

  // First sync with one tab and one window.
  TestSyncedWindowDelegate* window = AddWindow();
  TestSyncedTabDelegate* tab1 = AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(in, &out);
  ASSERT_TRUE(out[1].sync_data().GetSpecifics().session().has_tab());
  manager()->StopSyncing(syncer::SESSIONS);

  // Now create a second window with a placeholder that has the same sync id,
  // but a different tab id.
  TestSyncedWindowDelegate* window2 = AddWindow();
  int sync_id = out[1].sync_data().GetSpecifics().session().tab_node_id();
  PlaceholderTabDelegate tab2(SessionID().id(), sync_id);
  window2->OverrideTabAt(0, &tab2);

  // Resync, reusing the old sync data.
  in.push_back(CreateRemoteData(out[0].sync_data().GetSpecifics()));
  in.push_back(CreateRemoteData(out[1].sync_data().GetSpecifics()));
  out.clear();
  InitWithSyncDataTakeOutput(in, &out);

  // The tab entity will be overwritten twice. Once with the information for
  // tab 1 and then again with the information for tab 2. This will be followed
  // by a header change reflecting both tabs.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE,
                         SyncChange::ACTION_UPDATE}));
  VerifyLocalHeaderChange(out[2], 2, 2);
  VerifyLocalTabChange(out[0], 1, kFoo1);
  EXPECT_EQ(sync_id, out[0].sync_data().GetSpecifics().session().tab_node_id());
  EXPECT_EQ(tab1->GetSessionId(),
            out[0].sync_data().GetSpecifics().session().tab().tab_id());
  // Because tab 2 is a placeholder, tab 1's URL will be preserved.
  VerifyLocalTabChange(out[1], 1, kFoo1);
  EXPECT_EQ(sync_id, out[1].sync_data().GetSpecifics().session().tab_node_id());
  EXPECT_EQ(tab2.GetSessionId(),
            out[1].sync_data().GetSpecifics().session().tab().tab_id());
  EXPECT_EQ(window2->GetSessionId(),
            out[1].sync_data().GetSpecifics().session().tab().window_id());
}

}  // namespace sync_sessions
