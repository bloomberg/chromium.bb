// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_sync_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model/recording_model_type_change_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

namespace {

void VerifyEqual(const UserEventSpecifics& s1, const UserEventSpecifics& s2) {
  EXPECT_EQ(s1.event_time_usec(), s2.event_time_usec());
  EXPECT_EQ(s1.navigation_id(), s2.navigation_id());
  EXPECT_EQ(s1.session_id(), s2.session_id());
}

void VerifyDataBatchCount(int expected_count,
                          std::unique_ptr<DataBatch> batch) {
  int actual_count = 0;
  while (batch->HasNext()) {
    ++actual_count;
    batch->Next();
  }
  EXPECT_EQ(expected_count, actual_count);
}

void VerifyDataBatch(std::map<std::string, UserEventSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.user_event());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
}

base::Callback<void(std::unique_ptr<DataBatch> batch)> VerifyCallback(
    std::map<std::string, UserEventSpecifics> expected) {
  return base::Bind(&VerifyDataBatch, expected);
}

UserEventSpecifics CreateSpecifics(int64_t event_time_usec,
                                   int64_t navigation_id,
                                   uint64_t session_id) {
  UserEventSpecifics specifics;
  specifics.set_event_time_usec(event_time_usec);
  specifics.set_navigation_id(navigation_id);
  specifics.set_session_id(session_id);
  return specifics;
}

std::unique_ptr<UserEventSpecifics> SpecificsUniquePtr(int64_t event_time_usec,
                                                       int64_t navigation_id,
                                                       uint64_t session_id) {
  return std::make_unique<UserEventSpecifics>(
      CreateSpecifics(event_time_usec, navigation_id, session_id));
}

class TestGlobalIdMapper : public GlobalIdMapper {
 public:
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {
    callback_ = std::move(callback);
  }

  int64_t GetLatestGlobalId(int64_t global_id) override {
    auto iter = id_map_.find(global_id);
    return iter == id_map_.end() ? global_id : iter->second;
  }

  void ChangeId(int64_t old_id, int64_t new_id) {
    id_map_[old_id] = new_id;
    callback_.Run(old_id, new_id);
  }

 private:
  GlobalIdChange callback_;
  std::map<int64_t, int64_t> id_map_;
};

class UserEventSyncBridgeTest : public testing::Test {
 protected:
  UserEventSyncBridgeTest() {
    bridge_ = std::make_unique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        RecordingModelTypeChangeProcessor::FactoryForBridgeTest(&processor_),
        &test_global_id_mapper_);
  }

  ~UserEventSyncBridgeTest() override {
    // Get[All]Data() calls are async, so this will run the verification they
    // call in their callbacks.
    base::RunLoop().RunUntilIdle();
  }

  std::string GetStorageKey(const UserEventSpecifics& specifics) {
    EntityData entity_data;
    *entity_data.specifics.mutable_user_event() = specifics;
    return bridge()->GetStorageKey(entity_data);
  }

  UserEventSyncBridge* bridge() { return bridge_.get(); }
  RecordingModelTypeChangeProcessor* processor() { return processor_; }
  TestGlobalIdMapper* mapper() { return &test_global_id_mapper_; }

 private:
  std::unique_ptr<UserEventSyncBridge> bridge_;
  RecordingModelTypeChangeProcessor* processor_;
  TestGlobalIdMapper test_global_id_mapper_;
  base::MessageLoop message_loop_;
};

TEST_F(UserEventSyncBridgeTest, MetadataIsInitialized) {
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor()->metadata()->GetModelTypeState().initial_sync_done());
}

TEST_F(UserEventSyncBridgeTest, SingleRecord) {
  const UserEventSpecifics specifics(CreateSpecifics(1u, 2u, 3u));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));
  EXPECT_EQ(1u, processor()->put_multimap().size());

  const std::string storage_key = processor()->put_multimap().begin()->first;
  bridge()->GetData({storage_key}, VerifyCallback({{storage_key, specifics}}));
  bridge()->GetData({"bogus"}, base::Bind(&VerifyDataBatchCount, 0));
  bridge()->GetAllData(VerifyCallback({{storage_key, specifics}}));

  bridge()->DisableSync();

  // Disabling deletes records through multiple round trips, if we quickly call
  // GetAllData() we're going to beat the deletions to the storage task.
  base::RunLoop().RunUntilIdle();

  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 0));
  EXPECT_EQ(0u, processor()->delete_set().size());
}

TEST_F(UserEventSyncBridgeTest, MultipleRecords) {
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 1u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 2u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(2u, 2u, 2u));

  EXPECT_EQ(4u, processor()->put_multimap().size());
  std::set<std::string> unique_storage_keys;
  for (const auto& kv : processor()->put_multimap()) {
    unique_storage_keys.insert(kv.first);
  }
  EXPECT_EQ(2u, unique_storage_keys.size());
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 2));
}

TEST_F(UserEventSyncBridgeTest, ApplySyncChanges) {
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 1u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(2u, 2u, 2u));
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 2));

  const std::string storage_key = processor()->put_multimap().begin()->first;
  auto error_on_delete =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 {EntityChange::CreateDelete(storage_key)});
  EXPECT_FALSE(error_on_delete);
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 1));
}

TEST_F(UserEventSyncBridgeTest, HandleGlobalIdChange) {
  int64_t first_id = 11;
  int64_t second_id = 12;
  int64_t third_id = 13;
  int64_t fourth_id = 14;

  // This id update should be applied to the event as it is initially recorded.
  mapper()->ChangeId(first_id, second_id);
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, first_id, 2u));
  const std::string storage_key = processor()->put_multimap().begin()->first;
  EXPECT_EQ(1u, processor()->put_multimap().size());
  bridge()->GetAllData(
      VerifyCallback({{storage_key, CreateSpecifics(1u, second_id, 2u)}}));

  // This id update is done while the event is "in flight", and should result in
  // it being updated and re-sent to sync.
  mapper()->ChangeId(second_id, third_id);
  EXPECT_EQ(2u, processor()->put_multimap().size());
  bridge()->GetAllData(
      VerifyCallback({{storage_key, CreateSpecifics(1u, third_id, 2u)}}));
  auto error_on_delete =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 {EntityChange::CreateDelete(storage_key)});
  EXPECT_FALSE(error_on_delete);
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 0));

  // This id update should be ignored, since we received commit confirmation
  // above.
  mapper()->ChangeId(third_id, fourth_id);
  EXPECT_EQ(2u, processor()->put_multimap().size());
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 0));
}

TEST_F(UserEventSyncBridgeTest, MulipleEventsChanging) {
  int64_t first_id = 11;
  int64_t second_id = 12;
  int64_t third_id = 13;
  int64_t fourth_id = 14;
  const UserEventSpecifics specifics1 = CreateSpecifics(1u, first_id, 2u);
  const UserEventSpecifics specifics2 = CreateSpecifics(1u, first_id, 2u);
  const UserEventSpecifics specifics3 = CreateSpecifics(1u, first_id, 2u);
  const std::string key1 = GetStorageKey(specifics1);
  const std::string key2 = GetStorageKey(specifics2);
  const std::string key3 = GetStorageKey(specifics3);

  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics1));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics2));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics3));
  bridge()->GetAllData(VerifyCallback(
      {{key1, specifics1}, {key2, specifics2}, {key3, specifics3}}));

  mapper()->ChangeId(second_id, fourth_id);
  bridge()->GetAllData(
      VerifyCallback({{key1, specifics1},
                      {key2, CreateSpecifics(3u, fourth_id, 4u)},
                      {key3, specifics3}}));

  mapper()->ChangeId(first_id, fourth_id);
  mapper()->ChangeId(third_id, fourth_id);
  bridge()->GetAllData(
      VerifyCallback({{key1, CreateSpecifics(1u, fourth_id, 2u)},
                      {key2, CreateSpecifics(3u, fourth_id, 4u)},
                      {key3, CreateSpecifics(5u, fourth_id, 6u)}}));
}

TEST_F(UserEventSyncBridgeTest, RecordBeforeMetadataLoads) {
  processor()->SetIsTrackingMetadata(false);
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 2u, 3u));
  bridge()->GetAllData(VerifyCallback({}));
}

}  // namespace

}  // namespace syncer
