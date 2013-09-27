// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions2/sessions_sync_manager.h"

#include "base/run_loop.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/session_sync_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    : public testing::Test,
      public SessionsSyncManager::SyncInternalApiDelegate {
 public:
  SessionsSyncManagerTest() {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    manager_.reset(new SessionsSyncManager(
        profile_.get(),
        scoped_ptr<SyncPrefs>(new SyncPrefs(profile_->GetPrefs())),
        this));
  }

  virtual void TearDown() OVERRIDE {
    helper()->Reset();
    manager_.reset();
    profile_.reset();
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

  syncer::SyncChangeList* FilterOutLocalHeaderCreation(
      syncer::SyncChangeList* list) {
    syncer::SyncChangeList::iterator it = list->begin();
    bool found = false;
    for (; it != list->end(); ++it) {
      if (it->sync_data().GetTag() == manager_->current_machine_tag()) {
        EXPECT_EQ(syncer::SyncChange::ACTION_ADD, it->change_type());
        list->erase(it);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
    return list;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<SessionsSyncManager> manager_;
  SessionSyncTestHelper helper_;
};

TEST_F(SessionsSyncManagerTest, MergeLocalSession) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  EXPECT_FALSE(manager()->current_machine_tag().empty());

  EXPECT_EQ(1U, out.size());
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

  // Now take that header node and feed it in as input.
  SyncData d(SyncData::CreateRemoteData(1, data.GetSpecifics(), base::Time()));
  syncer::SyncDataList in(&d, &d + 1);
  out.clear();
  TearDown();
  SetUp();
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_TRUE(out.empty());
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
  EXPECT_TRUE(FilterOutLocalHeaderCreation(&output)->empty());

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
  TearDown();
  SetUp();
  InitWithSyncDataTakeOutput(in, &changes);
  EXPECT_TRUE(FilterOutLocalHeaderCreation(&changes)->empty());
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
  EXPECT_EQ(1U, FilterOutLocalHeaderCreation(&changes)->size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[0].change_type());
  EXPECT_EQ(TabNodePool2::TabIdToTag(local_tag, tab_node_id),
            changes[0].sync_data().GetTag());
}

}  // namespace browser_sync
