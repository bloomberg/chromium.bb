// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions2/sessions_sync_manager.h"

#include "base/strings/string_util.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/session_sync_test_helper.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;
using syncer::SyncData;

namespace browser_sync {

namespace {

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output) {}
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    if (output_)
      output_->assign(change_list.begin(), change_list.end());
    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE {
    return syncer::SyncDataList();
  }
 private:
  syncer::SyncChangeList* output_;
};

}  // namespace

class SessionsSyncManagerTest
    : public BrowserWithTestWindowTest,
      public SessionsSyncManager::SyncInternalApiDelegate {
 public:
  SessionsSyncManagerTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    manager_.reset(new SessionsSyncManager(
        profile(),
        scoped_ptr<SyncPrefs>(new SyncPrefs(profile()->GetPrefs())),
        this));
  }

  virtual void TearDown() OVERRIDE {
    helper()->Reset();
    manager_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  virtual scoped_ptr<DeviceInfo> GetLocalDeviceInfo() const OVERRIDE {
    return scoped_ptr<DeviceInfo>(
        new DeviceInfo(GetCacheGuid(),
                       "Wayne Gretzky's Hacking Box",
                       "Chromium 10k",
                       "Chrome 10k",
                       sync_pb::SyncEnums_DeviceType_TYPE_LINUX));
  }

  virtual std::string GetCacheGuid() const OVERRIDE {
    return "cache_guid";
  }

  SessionsSyncManager* manager() { return manager_.get(); }
  SessionSyncTestHelper* helper() { return &helper_; }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    syncer::SyncMergeResult result = manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, initial_data,
        scoped_ptr<syncer::SyncChangeProcessor>(
            new TestSyncProcessorStub(output)),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), NULL);
  }

  syncer::SyncChangeList* FilterOutLocalHeaderChanges(
      syncer::SyncChangeList* list) {
    syncer::SyncChangeList::iterator it = list->begin();
    bool found = false;
    while (it != list->end()) {
      if (it->sync_data().GetTag() == manager_->current_machine_tag()) {
        EXPECT_TRUE(syncer::SyncChange::ACTION_ADD == it->change_type() ||
                    syncer::SyncChange::ACTION_UPDATE == it->change_type());
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
};

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
                            is_managed_(false),
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
  virtual bool ProfileIsManaged() const OVERRIDE {
   return is_managed_;
  }
  void set_is_managed(bool is_managed) { is_managed_ = is_managed; }
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
   return -1;
  }
  virtual void SetSyncId(int sync_id) OVERRIDE {}

  void reset() {
    current_entry_index_ = 0;
    pending_entry_index_ = -1;
    entries_.clear();
  }

 private:
   int current_entry_index_;
   int pending_entry_index_;
   bool is_managed_;
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
  EXPECT_FALSE(manager()->ShouldSyncTab(tab));
  tab.reset();

  // A chrome:// entry isn't valid.
  content::NavigationEntry* entry(content::NavigationEntry::Create());
  entry->SetVirtualURL(GURL("chrome://preferences/"));
  tab.AppendEntry(entry);
  EXPECT_FALSE(manager()->ShouldSyncTab(tab));


  // A file:// entry isn't valid, even in addition to another entry.
  content::NavigationEntry* entry2(content::NavigationEntry::Create());
  entry2->SetVirtualURL(GURL("file://bla"));
  tab.AppendEntry(entry2);
  EXPECT_FALSE(manager()->ShouldSyncTab(tab));

  // Add a valid scheme entry to tab, making the tab valid.
  content::NavigationEntry* entry3(content::NavigationEntry::Create());
  entry3->SetVirtualURL(GURL("http://www.google.com"));
  tab.AppendEntry(entry3);
  EXPECT_FALSE(manager()->ShouldSyncTab(tab));
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

// Tests that for managed users blocked navigations are recorded and marked as
// such, while regular navigations are marked as allowed.
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

  tab.set_is_managed(true);
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

TEST_F(SessionsSyncManagerTest, MergeLocalSessionNoTabs) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  EXPECT_FALSE(manager()->current_machine_tag().empty());

  EXPECT_EQ(2U, out.size());
  EXPECT_TRUE(out[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, out[0].change_type());
  const SyncData data(out[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(), data.GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_TRUE(header_s.has_device_type());
  EXPECT_EQ(GetLocalDeviceInfo()->client_name(), header_s.client_name());
  EXPECT_EQ(0, header_s.window_size());

  EXPECT_TRUE(out[1].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[1].change_type());
  const SyncData data_2(out[1].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(), data_2.GetTag());
  const sync_pb::SessionSpecifics& specifics2(data_2.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics2.session_tag());
  EXPECT_TRUE(specifics2.has_header());
  const sync_pb::SessionHeader& header_s2 = specifics2.header();
  EXPECT_EQ(0, header_s2.window_size());

  // Now take that header node and feed it in as input.
  SyncData d(SyncData::CreateRemoteData(1, data.GetSpecifics(), base::Time()));
  syncer::SyncDataList in(&d, &d + 1);
  out.clear();
  SessionsSyncManager manager2(
      profile(),
      scoped_ptr<SyncPrefs>(new SyncPrefs(profile()->GetPrefs())),
      this);
  syncer::SyncMergeResult result = manager2.MergeDataAndStartSyncing(
      syncer::SESSIONS, in,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new TestSyncProcessorStub(&out)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));
  ASSERT_FALSE(result.error().IsSet());

  EXPECT_EQ(1U, out.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[0].change_type());
  EXPECT_TRUE(out[0].sync_data().GetSpecifics().session().has_header());
}

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
  initial_data.push_back(SyncData::CreateRemoteData(1, entity, base::Time()));
  for (size_t i = 0; i < tabs1.size(); i++) {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(tabs1[i]);
    initial_data.push_back(SyncData::CreateRemoteData(
        i + 2, entity, base::Time()));
  }
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(tag, 0, tab_list2[i],
                                entity.mutable_session());
    initial_data.push_back(
        SyncData::CreateRemoteData(i + 10, entity, base::Time()));
  }

  syncer::SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
  EXPECT_TRUE(FilterOutLocalHeaderChanges(&output)->empty());

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  session_reference.push_back(tab_list2);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

TEST_F(SessionsSyncManagerTest, DeleteForeignSession) {
  InitWithNoSyncData();
  std::string tag = "tag1";
  syncer::SyncChangeList changes;

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  manager()->DeleteForeignSession(tag, &changes);
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
  manager()->DeleteForeignSession(tag, &changes);
  EXPECT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));

  EXPECT_EQ(5U, changes.size());
  std::set<std::string> expected_tags(&tag, &tag + 1);
  for (int i = 0; i < 5; i++)
    expected_tags.insert(TabNodePool2::TabIdToTag(tag, i));

  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(changes[i].ToString());
    EXPECT_TRUE(changes[i].IsValid());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[i].change_type());
    EXPECT_TRUE(changes[i].sync_data().IsValid());
    EXPECT_EQ(1U, expected_tags.erase(changes[i].sync_data().GetTag()));
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

  // Add tabs for first window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    manager()->UpdateTrackerWithForeignSession(*iter, base::Time());
  }
  // Update associator with the session's meta node containing one window.
  manager()->UpdateTrackerWithForeignSession(meta, base::Time());

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

  // Update associator with the session's meta node containing two windows.
  manager()->UpdateTrackerWithForeignSession(meta, base::Time());
  // Add tabs for first window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    manager()->UpdateTrackerWithForeignSession(*iter, base::Time());
  }
  // Add tabs for second window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs2.begin();
       iter != tabs2.end(); ++iter) {
    manager()->UpdateTrackerWithForeignSession(*iter, base::Time());
  }

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

  // Update associator with the session's meta node containing one window.
  manager()->UpdateTrackerWithForeignSession(meta, base::Time());

  // Check that the foreign session was associated and retrieve the data.
  foreign_sessions.clear();
  ASSERT_TRUE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
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
  SyncData d(SyncData::CreateRemoteData(1, entity, base::Time()));
  syncer::SyncDataList in(&d, &d + 1);
  changes.clear();
  SessionsSyncManager manager2(
      profile(),
      scoped_ptr<SyncPrefs>(new SyncPrefs(profile()->GetPrefs())),
      this);
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
      1, changes[0].sync_data().GetSpecifics(), base::Time()));
  syncer::SyncDataList in(&d, &d + 1);
  changes.clear();
  TearDown();
  SetUp();
  InitWithSyncDataTakeOutput(in, &changes);
  EXPECT_EQ(1U, FilterOutLocalHeaderChanges(&changes)->size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[0].change_type());
  EXPECT_EQ(TabNodePool2::TabIdToTag(local_tag, tab_node_id),
            changes[0].sync_data().GetTag());
}

TEST_F(SessionsSyncManagerTest, AssociateWindowsDontReloadTabs) {
  syncer::SyncChangeList out;
  // Go to a URL that is ignored by session syncing.
  AddTab(browser(), GURL("chrome://preferences/"));
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(2U, out.size());
  EXPECT_EQ(
      0,
      out[1].sync_data().GetSpecifics().session().header().window_size());
  out.clear();

  // Go to a sync-interesting URL.
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  // Simulate a selective association (e.g in response to tab event) as
  // would occur in practice from ProcessSyncChanges.
  content::WebContents* c =
      browser()->tab_strip_model()->GetActiveWebContents();
  manager()->AssociateTab(SyncedTabDelegate::ImplFromWebContents(c), &out);
  ASSERT_EQ(2U, out.size());

  EXPECT_TRUE(StartsWithASCII(out[0].sync_data().GetTag(),
                              manager()->current_machine_tag(), true));
  EXPECT_EQ(manager()->current_machine_tag(),
            out[0].sync_data().GetSpecifics().session().session_tag());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, out[0].change_type());

  EXPECT_TRUE(StartsWithASCII(out[1].sync_data().GetTag(),
                              manager()->current_machine_tag(), true));
  EXPECT_EQ(manager()->current_machine_tag(),
            out[1].sync_data().GetSpecifics().session().session_tag());
  EXPECT_TRUE(out[1].sync_data().GetSpecifics().session().has_tab());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[1].change_type());

  out.clear();
  manager()->AssociateWindows(SessionsSyncManager::DONT_RELOAD_TABS,
                              &out);

  EXPECT_EQ(1U, out.size());
  EXPECT_TRUE(out[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[0].change_type());
  const SyncData data(out[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(), data.GetTag());
  const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
  EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
  EXPECT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  EXPECT_EQ(1, header_s.window_size());
  EXPECT_EQ(1, header_s.window(0).tab_size());
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionExistingTabs) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(6U, out.size());

  // Check that this machine's data is not included in the foreign windows.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(foreign_sessions.size(), 0U);

  // Verify the header.
  EXPECT_TRUE(out[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, out[0].change_type());
  const SyncData data(out[0].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(), data.GetTag());
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
    EXPECT_TRUE(StartsWithASCII(data.GetTag(),
                                manager()->current_machine_tag(), true));
    const sync_pb::SessionSpecifics& specifics(data.GetSpecifics().session());
    EXPECT_EQ(manager()->current_machine_tag(), specifics.session_tag());
    if (i % 2 == 1) {
      EXPECT_EQ(syncer::SyncChange::ACTION_ADD, out[i].change_type());
    } else {
      EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[i].change_type());
      EXPECT_TRUE(specifics.has_tab());
    }
  }

  // Verify the header was updated to reflect new window state.
  EXPECT_TRUE(out[5].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[5].change_type());
  const SyncData data_2(out[5].sync_data());
  EXPECT_EQ(manager()->current_machine_tag(), data_2.GetTag());
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

TEST_F(SessionsSyncManagerTest, CheckPrerenderedWebContentsSwap) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  ASSERT_EQ(4U, out.size());  // Header, tab ADD, tab UPDATE, header UPDATE.

  // To simulate WebContents swap during prerendering, create new WebContents
  // and swap with old WebContents.
  content::WebContents* old_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create new WebContents, with the required tab helpers.
  WebContents* new_web_contents = WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(profile()),
      old_web_contents->GetController().GetSessionStorageNamespaceMap());
  SessionTabHelper::CreateForWebContents(new_web_contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(new_web_contents);
  new_web_contents->GetController()
      .CopyStateFrom(old_web_contents->GetController());

  // Swap the WebContents.
  int index =
      browser()->tab_strip_model()->GetIndexOfWebContents(old_web_contents);
  browser()->tab_strip_model()->ReplaceWebContentsAt(index, new_web_contents);

  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);
  ASSERT_EQ(7U, out.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, out[4].change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, out[5].change_type());

  // Navigate away.
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);

  // Delete old WebContents. This should not crash.
  delete old_web_contents;
  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);

  // Try more navigations and verify output size. This can also reveal
  // bugs (leaks) on memcheck bots if the SessionSyncManager
  // didn't properly clean up the tab pool or session tracker.
  NavigateAndCommitActiveTab(GURL("http://bar3"));
  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);

  AddTab(browser(), GURL("http://bar4"));
  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);
  NavigateAndCommitActiveTab(GURL("http://bar5"));
  manager()->AssociateWindows(SessionsSyncManager::RELOAD_TABS, &out);
  ASSERT_EQ(20U, out.size());
}

}  // namespace browser_sync
