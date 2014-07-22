// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sessions_sync_manager.h"

#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/local_device_info_provider_mock.h"
#include "chrome/browser/sync/glue/session_sync_test_helper.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/sync/sessions/sessions_util.h"
#include "chrome/browser/sync/sessions/synced_window_delegates_getter.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_service_proxy_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;
using syncer::SyncChange;
using syncer::SyncData;

namespace browser_sync {

namespace {

class SyncedWindowDelegateOverride : public SyncedWindowDelegate {
 public:
  explicit SyncedWindowDelegateOverride(SyncedWindowDelegate* wrapped)
      : wrapped_(wrapped) {
  }
  virtual ~SyncedWindowDelegateOverride() {}

  virtual bool HasWindow() const OVERRIDE {
    return wrapped_->HasWindow();
  }

  virtual SessionID::id_type GetSessionId() const OVERRIDE {
    return wrapped_->GetSessionId();
  }

  virtual int GetTabCount() const OVERRIDE {
    return wrapped_->GetTabCount();
  }

  virtual int GetActiveIndex() const OVERRIDE {
    return wrapped_->GetActiveIndex();
  }

  virtual bool IsApp() const OVERRIDE {
    return wrapped_->IsApp();
  }

  virtual bool IsTypeTabbed() const OVERRIDE {
    return wrapped_->IsTypeTabbed();
  }

  virtual bool IsTypePopup() const OVERRIDE {
    return wrapped_->IsTypePopup();
  }

  virtual bool IsTabPinned(const SyncedTabDelegate* tab) const OVERRIDE {
    return wrapped_->IsTabPinned(tab);
  }

  virtual SyncedTabDelegate* GetTabAt(int index) const OVERRIDE {
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

  virtual SessionID::id_type GetTabIdAt(int index) const OVERRIDE {
    if (tab_id_overrides_.find(index) != tab_id_overrides_.end())
      return tab_id_overrides_.find(index)->second;
    return wrapped_->GetTabIdAt(index);
  }

  virtual bool IsSessionRestoreInProgress() const OVERRIDE {
    return wrapped_->IsSessionRestoreInProgress();
  }

 private:
  std::map<int, SyncedTabDelegate*> tab_overrides_;
  std::map<int, SessionID::id_type> tab_id_overrides_;
  SyncedWindowDelegate* wrapped_;
};

class TestSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter(
      const std::set<SyncedWindowDelegate*>& delegates)
      : delegates_(delegates) {}

  virtual const std::set<SyncedWindowDelegate*> GetSyncedWindowDelegates()
      OVERRIDE {
    return delegates_;
  }
 private:
  const std::set<SyncedWindowDelegate*> delegates_;
};

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output) {}
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    if (error_.IsSet()) {
      syncer::SyncError error = error_;
      error_ = syncer::SyncError();
      return error;
    }

    if (output_)
      output_->insert(output_->end(), change_list.begin(), change_list.end());

    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE {
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

syncer::SyncChange MakeRemoteChange(
    int64 id,
    const sync_pb::SessionSpecifics& specifics,
    SyncChange::SyncChangeType type) {
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(specifics);
  return syncer::SyncChange(
      FROM_HERE,
      type,
      syncer::SyncData::CreateRemoteData(
          id,
          entity,
          base::Time(),
          syncer::AttachmentIdList(),
          syncer::AttachmentServiceProxyForTest::Create()));
}

void AddTabsToChangeList(
      const std::vector<sync_pb::SessionSpecifics>& batch,
      SyncChange::SyncChangeType type,
      syncer::SyncChangeList* change_list) {
  std::vector<sync_pb::SessionSpecifics>::const_iterator iter;
  for (iter = batch.begin();
       iter != batch.end(); ++iter) {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(*iter);
    change_list->push_back(syncer::SyncChange(
        FROM_HERE,
        type,
        syncer::SyncData::CreateRemoteData(
            iter->tab_node_id(),
            entity,
            base::Time(),
            syncer::AttachmentIdList(),
            syncer::AttachmentServiceProxyForTest::Create())));
  }
}

void AddTabsToSyncDataList(const std::vector<sync_pb::SessionSpecifics> tabs,
                           syncer::SyncDataList* list) {
  for (size_t i = 0; i < tabs.size(); i++) {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(tabs[i]);
    list->push_back(SyncData::CreateRemoteData(
        i + 2,
        entity,
        base::Time(),
        syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create()));
  }
}

class DummyRouter : public LocalSessionEventRouter {
 public:
  virtual ~DummyRouter() {}
  virtual void StartRoutingTo(LocalSessionEventHandler* handler) OVERRIDE {}
  virtual void Stop() OVERRIDE {}
};

scoped_ptr<LocalSessionEventRouter> NewDummyRouter() {
  return scoped_ptr<LocalSessionEventRouter>(new DummyRouter());
}

}  // namespace

class SessionsSyncManagerTest
    : public BrowserWithTestWindowTest {
 public:
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

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    browser_sync::NotificationServiceSessionsRouter* router(
        new browser_sync::NotificationServiceSessionsRouter(
            profile(), syncer::SyncableService::StartSyncFlare()));
    manager_.reset(new SessionsSyncManager(profile(), local_device_.get(),
      scoped_ptr<LocalSessionEventRouter>(router)));
  }

  virtual void TearDown() OVERRIDE {
    test_processor_ = NULL;
    helper()->Reset();
    manager_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  const DeviceInfo* GetLocalDeviceInfo() {
    return local_device_->GetLocalDeviceInfo();
  }

  SessionsSyncManager* manager() { return manager_.get(); }
  SessionSyncTestHelper* helper() { return &helper_; }
  LocalDeviceInfoProvider* local_device() { return local_device_.get(); }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    test_processor_ = new TestSyncProcessorStub(output);
    syncer::SyncMergeResult result = manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, initial_data,
        scoped_ptr<syncer::SyncChangeProcessor>(test_processor_),
        scoped_ptr<syncer::SyncErrorFactory>(
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
      if (syncer::SyncDataLocal(it->sync_data()).GetTag() ==
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

 private:
  scoped_ptr<SessionsSyncManager> manager_;
  SessionSyncTestHelper helper_;
  TestSyncProcessorStub* test_processor_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
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
  ASSERT_EQ(1, session->windows[0]->type);
  ASSERT_EQ(1U, manager()->session_tracker_.num_synced_sessions());
  ASSERT_EQ(1U,
            manager()->session_tracker_.num_synced_tabs(std::string("tag")));
}

namespace {

class SyncedTabDelegateFake : public SyncedTabDelegate {
 public:
  SyncedTabDelegateFake() : current_entry_index_(0),
                            pending_entry_index_(-1),
                            is_supervised_(false),
                            sync_id_(-1),
                            blocked_navigations_(NULL) {}
  virtual ~SyncedTabDelegateFake() {}

  virtual int GetCurrentEntryIndex() const OVERRIDE {
    return current_entry_index_;
  }
  void set_current_entry_index(int i) {
    current_entry_index_ = i;
  }

  virtual content::NavigationEntry* GetEntryAtIndex(int i) const OVERRIDE {
    const int size = entries_.size();
    return (size < i + 1) ? NULL : entries_[i];
  }

  void AppendEntry(content::NavigationEntry* entry) {
    entries_.push_back(entry);
  }

  virtual int GetEntryCount() const OVERRIDE {
    return entries_.size();
  }

  virtual int GetPendingEntryIndex() const OVERRIDE {
    return pending_entry_index_;
  }
  void set_pending_entry_index(int i) {
    pending_entry_index_ = i;
  }

  virtual SessionID::id_type GetWindowId() const OVERRIDE {
    return SessionID::id_type();
  }

  virtual SessionID::id_type GetSessionId() const OVERRIDE {
    return SessionID::id_type();
  }

  virtual bool IsBeingDestroyed() const OVERRIDE { return false; }
  virtual Profile* profile() const OVERRIDE { return NULL; }
  virtual std::string GetExtensionAppId() const OVERRIDE {
    return std::string();
  }
  virtual content::NavigationEntry* GetPendingEntry() const OVERRIDE {
   return NULL;
  }
  virtual content::NavigationEntry* GetActiveEntry() const OVERRIDE {
   return NULL;
  }
  virtual bool ProfileIsSupervised() const OVERRIDE {
   return is_supervised_;
  }
  void set_is_supervised(bool is_supervised) { is_supervised_ = is_supervised; }
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const OVERRIDE {
    return blocked_navigations_;
  }
  void set_blocked_navigations(
      std::vector<const content::NavigationEntry*>* navs) {
    blocked_navigations_ = navs;
  }
  virtual bool IsPinned() const OVERRIDE {
   return false;
  }
  virtual bool HasWebContents() const OVERRIDE {
   return false;
  }
  virtual content::WebContents* GetWebContents() const OVERRIDE {
   return NULL;
  }

  // Session sync related methods.
  virtual int GetSyncId() const OVERRIDE {
   return sync_id_;
  }
  virtual void SetSyncId(int sync_id) OVERRIDE {
    sync_id_ = sync_id;
  }

  void reset() {
    current_entry_index_ = 0;
    pending_entry_index_ = -1;
    sync_id_ = -1;
    entries_.clear();
  }

 private:
   int current_entry_index_;
   int pending_entry_index_;
   bool is_supervised_;
   int sync_id_;
   std::vector<const content::NavigationEntry*>* blocked_navigations_;
   ScopedVector<content::NavigationEntry> entries_;
};

}  // namespace

// Test that we exclude tabs with only chrome:// and file:// schemed navigations
// from ShouldSyncTab(..).
TEST_F(SessionsSyncManagerTest, ValidTabs) {
  SyncedTabDelegateFake tab;

  // A null entry shouldn't crash.
  tab.AppendEntry(NULL);
  EXPECT_FALSE(sessions_util::ShouldSyncTab(tab));
  tab.reset();

  // A chrome:// entry isn't valid.
  content::NavigationEntry* entry(content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("chrome://preferences/"));
  tab.AppendEntry(entry);
  EXPECT_FALSE(sessions_util::ShouldSyncTab(tab));


  // A file:// entry isn't valid, even in addition to another entry.
  content::NavigationEntry* entry2(content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("file://bla"));
  tab.AppendEntry(entry2);
  EXPECT_FALSE(sessions_util::ShouldSyncTab(tab));

  // Add a valid scheme entry to tab, making the tab valid.
  content::NavigationEntry* entry3(content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.google.com"));
  tab.AppendEntry(entry3);
  EXPECT_FALSE(sessions_util::ShouldSyncTab(tab));
}

// Make sure GetCurrentVirtualURL() returns the virtual URL of the pending
// entry if the current entry is pending.
TEST_F(SessionsSyncManagerTest, GetCurrentVirtualURLPending) {
  SyncedTabDelegateFake tab;
  content::NavigationEntry* entry(content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("http://www.google.com"));
  tab.AppendEntry(entry);
  EXPECT_EQ(entry->GetVirtualURL(), manager()->GetCurrentVirtualURL(tab));
}

// Make sure GetCurrentVirtualURL() returns the virtual URL of the current
// entry if the current entry is non-pending.
TEST_F(SessionsSyncManagerTest, GetCurrentVirtualURLNonPending) {
  SyncedTabDelegateFake tab;
  content::NavigationEntry* entry(content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("http://www.google.com"));
  tab.AppendEntry(entry);
  EXPECT_EQ(entry->GetVirtualURL(), manager()->GetCurrentVirtualURL(tab));
}

static const base::Time kTime1 = base::Time::FromInternalValue(100);
static const base::Time kTime2 = base::Time::FromInternalValue(105);
static const base::Time kTime3 = base::Time::FromInternalValue(110);
static const base::Time kTime4 = base::Time::FromInternalValue(120);
static const base::Time kTime5 = base::Time::FromInternalValue(130);

// Populate the mock tab delegate with some data and navigation
// entries and make sure that setting a SessionTab from it preserves
// those entries (and clobbers any existing data).
TEST_F(SessionsSyncManagerTest, SetSessionTabFromDelegate) {
  // Create a tab with three valid entries.
  SyncedTabDelegateFake tab;
  content::NavigationEntry* entry1(content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  entry1->SetTimestamp(kTime1);
  entry1->SetHttpStatusCode(200);
  content::NavigationEntry* entry2(content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("http://www.noodle.com"));
  entry2->SetTimestamp(kTime2);
  entry2->SetHttpStatusCode(201);
  content::NavigationEntry* entry3(content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.doodle.com"));
  entry3->SetTimestamp(kTime3);
  entry3->SetHttpStatusCode(202);

  tab.AppendEntry(entry1);
  tab.AppendEntry(entry2);
  tab.AppendEntry(entry3);
  tab.set_current_entry_index(2);

  SessionTab session_tab;
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
  EXPECT_EQ(entry1->GetVirtualURL(),
            session_tab.navigations[0].virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL(),
            session_tab.navigations[1].virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL(),
            session_tab.navigations[2].virtual_url());
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

// Tests that for supervised users blocked navigations are recorded and marked
// as such, while regular navigations are marked as allowed.
TEST_F(SessionsSyncManagerTest, BlockedNavigations) {
  SyncedTabDelegateFake tab;
  content::NavigationEntry* entry1(content::NavigationEntry::Create());
  entry1->SetVirtualURL(GURL("http://www.google.com"));
  entry1->SetTimestamp(kTime1);
  tab.AppendEntry(entry1);

  content::NavigationEntry* entry2 = content::NavigationEntry::Create();
  entry2->SetVirtualURL(GURL("http://blocked.com/foo"));
  entry2->SetTimestamp(kTime2);
  content::NavigationEntry* entry3 = content::NavigationEntry::Create();
  entry3->SetVirtualURL(GURL("http://evil.com"));
  entry3->SetTimestamp(kTime3);
  ScopedVector<const content::NavigationEntry> blocked_navigations;
  blocked_navigations.push_back(entry2);
  blocked_navigations.push_back(entry3);

  tab.set_is_supervised(true);
  tab.set_blocked_navigations(&blocked_navigations.get());

  SessionTab session_tab;
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
  EXPECT_EQ(entry1->GetVirtualURL(),
            session_tab.navigations[0].virtual_url());
  EXPECT_EQ(entry2->GetVirtualURL(),
            session_tab.navigations[1].virtual_url());
  EXPECT_EQ(entry3->GetVirtualURL(),
            session_tab.navigations[2].virtual_url());
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
  SessionsSyncManager manager2(profile(), local_device(), NewDummyRouter());
  syncer::SyncMergeResult result = manager2.MergeDataAndStartSyncing(
      syncer::SESSIONS, in,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new TestSyncProcessorStub(&out)),
      scoped_ptr<syncer::SyncErrorFactory>(
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

  const std::set<SyncedWindowDelegate*> windows(
      SyncedWindowDelegate::GetSyncedWindowDelegates());
  ASSERT_EQ(1U, windows.size());
  SyncedTabDelegateFake t1_override, t2_override;
  t1_override.SetSyncId(1);  // No WebContents by default.
  t2_override.SetSyncId(2);  // No WebContents by default.
  SyncedWindowDelegateOverride window_override(*windows.begin());
  window_override.OverrideTabAt(1, &t1_override, kNewTabId);
  window_override.OverrideTabAt(2, &t2_override,
                                t2.GetSpecifics().session().tab().tab_id());
  std::set<SyncedWindowDelegate*> delegates;
  delegates.insert(&window_override);
  scoped_ptr<TestSyncedWindowDelegatesGetter> getter(
      new TestSyncedWindowDelegatesGetter(delegates));
  manager()->synced_window_getter_.reset(getter.release());

  syncer::SyncMergeResult result = manager()->MergeDataAndStartSyncing(
      syncer::SESSIONS, in,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new TestSyncProcessorStub(&out)),
      scoped_ptr<syncer::SyncErrorFactory>(
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
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i],
                                entity.mutable_session());
    initial_data.push_back(SyncData::CreateRemoteData(
        i + 10,
        entity,
        base::Time(),
        syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create()));
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
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  foreign_data.push_back(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
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
    EXPECT_TRUE(StartsWithASCII(syncer::SyncDataLocal(data).GetTag(),
                                manager()->current_machine_tag(), true));
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
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta1);
  foreign_data1.push_back(SyncData::CreateRemoteData(
      1,
      entity,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
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
  std::vector<SessionID::id_type> tab_list1 (nums1, nums1 + arraysize(nums1));
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
  std::vector<SessionID::id_type> tab_list1 (nums1, nums1 + arraysize(nums1));
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
  SessionsSyncManager manager2(profile(), local_device(), NewDummyRouter());
  syncer::SyncMergeResult result = manager2.MergeDataAndStartSyncing(
      syncer::SESSIONS, in,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new TestSyncProcessorStub(&changes)),
      scoped_ptr<syncer::SyncErrorFactory>(
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
      StartsWithASCII(syncer::SyncDataLocal(out[0].sync_data()).GetTag(),
                      manager()->current_machine_tag(),
                      true));
  EXPECT_EQ(manager()->current_machine_tag(),
            out[0].sync_data().GetSpecifics().session().session_tag());
  EXPECT_EQ(SyncChange::ACTION_ADD, out[0].change_type());

  EXPECT_TRUE(
      StartsWithASCII(syncer::SyncDataLocal(out[1].sync_data()).GetTag(),
                      manager()->current_machine_tag(),
                      true));
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
    EXPECT_TRUE(StartsWithASCII(syncer::SyncDataLocal(data).GetTag(),
                                manager()->current_machine_tag(), true));
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
    EXPECT_TRUE(StartsWithASCII(syncer::SyncDataLocal(data).GetTag(),
                                manager()->current_machine_tag(), true));
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
  EXPECT_EQ(GURL("http://foo1"), iter->second->tab()->
          GetEntryAtIndex(0)->GetVirtualURL());
  EXPECT_EQ(GURL("http://foo2"), iter->second->tab()->
          GetEntryAtIndex(1)->GetVirtualURL());
  iter++;
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  EXPECT_EQ(GURL("http://bar1"), iter->second->tab()->
      GetEntryAtIndex(0)->GetVirtualURL());
  EXPECT_EQ(GURL("http://bar2"), iter->second->tab()->
      GetEntryAtIndex(1)->GetVirtualURL());
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
  sync_pb::EntitySpecifics entity1, entity2;
  entity1.mutable_session()->CopyFrom(meta);
  entity2.mutable_session()->CopyFrom(meta2);
  foreign_data.push_back(SyncData::CreateRemoteData(
      1,
      entity1,
      tag1_time,
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  foreign_data.push_back(SyncData::CreateRemoteData(
      1,
      entity2,
      tag2_time,
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
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
  sync_pb::EntitySpecifics entity1;
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  entity1.mutable_session()->CopyFrom(meta);
  foreign_data.push_back(SyncData::CreateRemoteData(
      1,
      entity1,
      tag1_time,
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());

  // Update to a non-stale time.
  sync_pb::EntitySpecifics update_entity;
  update_entity.mutable_session()->CopyFrom(tabs1[0]);
  syncer::SyncChangeList changes;
  changes.push_back(
      syncer::SyncChange(FROM_HERE,
                         SyncChange::ACTION_UPDATE,
                         syncer::SyncData::CreateRemoteData(
                             1,
                             update_entity,
                             base::Time::Now(),
                             syncer::AttachmentIdList(),
                             syncer::AttachmentServiceProxyForTest::Create())));
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
  scoped_ptr<content::WebContents> old_web_contents;
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

namespace {
class SessionNotificationObserver : public content::NotificationObserver {
 public:
  SessionNotificationObserver() : notified_of_update_(false),
                                  notified_of_refresh_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                   content::NotificationService::AllSources());
  }
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED:
        notified_of_update_ = true;
        break;
      case chrome::NOTIFICATION_SYNC_REFRESH_LOCAL:
        notified_of_refresh_ = true;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  bool notified_of_update() const { return notified_of_update_; }
  bool notified_of_refresh() const { return notified_of_refresh_; }
  void Reset() {
    notified_of_update_ = false;
    notified_of_refresh_ = false;
  }
 private:
  content::NotificationRegistrar registrar_;
  bool notified_of_update_;
  bool notified_of_refresh_;
};
}  // namespace

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when processing
// sync changes.
TEST_F(SessionsSyncManagerTest, NotifiedOfUpdates) {
  SessionNotificationObserver observer;
  ASSERT_FALSE(observer.notified_of_update());
  InitWithNoSyncData();

  SessionID::id_type n[] = {5};
  std::vector<sync_pb::SessionSpecifics> tabs1;
  std::vector<SessionID::id_type> tab_list(n, n + arraysize(n));
  sync_pb::SessionSpecifics meta(helper()->BuildForeignSession(
      "tag1", tab_list, &tabs1));

  syncer::SyncChangeList changes;
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer.notified_of_update());

  changes.clear();
  observer.Reset();
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer.notified_of_update());

  changes.clear();
  observer.Reset();
  changes.push_back(MakeRemoteChange(1, meta, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(observer.notified_of_update());
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

  SessionNotificationObserver observer;
  ASSERT_FALSE(observer.notified_of_update());
  manager()->DeleteForeignSession(tag);
  ASSERT_TRUE(observer.notified_of_update());
}

#if defined(OS_ANDROID) || defined(OS_IOS)
// Tests that opening the other devices page triggers a session sync refresh.
// This page only exists on mobile platforms today; desktop has a
// search-enhanced NTP without other devices.
TEST_F(SessionsSyncManagerTest, NotifiedOfRefresh) {
  SessionNotificationObserver observer;
  ASSERT_FALSE(observer.notified_of_refresh());
  InitWithNoSyncData();
  AddTab(browser(), GURL("http://foo1"));
  EXPECT_FALSE(observer.notified_of_refresh());
  NavigateAndCommitActiveTab(GURL("chrome://newtab/#open_tabs"));
  EXPECT_TRUE(observer.notified_of_refresh());
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace browser_sync
