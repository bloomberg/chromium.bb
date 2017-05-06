// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_sync_bridge.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
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
  return base::MakeUnique<UserEventSpecifics>(
      CreateSpecifics(event_time_usec, navigation_id, session_id));
}

class UserEventSyncBridgeTest : public testing::Test {
 protected:
  UserEventSyncBridgeTest() {
    bridge_ = base::MakeUnique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        RecordingModelTypeChangeProcessor::FactoryForBridgeTest(&processor_));
  }

  UserEventSyncBridge* bridge() { return bridge_.get(); }
  const RecordingModelTypeChangeProcessor& processor() { return *processor_; }

 private:
  std::unique_ptr<UserEventSyncBridge> bridge_;
  RecordingModelTypeChangeProcessor* processor_;
  base::MessageLoop message_loop_;
};

TEST_F(UserEventSyncBridgeTest, MetadataIsInitialized) {
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor().metadata()->GetModelTypeState().initial_sync_done());
}

TEST_F(UserEventSyncBridgeTest, SingleRecord) {
  const UserEventSpecifics specifics(CreateSpecifics(1u, 2u, 3u));
  bridge()->RecordUserEvent(base::MakeUnique<UserEventSpecifics>(specifics));
  EXPECT_EQ(1u, processor().put_multimap().size());

  const std::string storage_key = processor().put_multimap().begin()->first;
  bridge()->GetData({storage_key}, VerifyCallback({{storage_key, specifics}}));
  bridge()->GetData({"bogus"}, base::Bind(&VerifyDataBatchCount, 0));
  bridge()->GetAllData(VerifyCallback({{storage_key, specifics}}));

  bridge()->DisableSync();
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 0));
  EXPECT_EQ(0u, processor().delete_set().size());
}

TEST_F(UserEventSyncBridgeTest, MultipleRecords) {
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 1u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 2u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(2u, 2u, 2u));

  EXPECT_EQ(4u, processor().put_multimap().size());
  std::set<std::string> unique_storage_keys;
  for (const auto& kv : processor().put_multimap()) {
    unique_storage_keys.insert(kv.first);
  }
  EXPECT_EQ(2u, unique_storage_keys.size());
  bridge()->GetAllData(base::Bind(&VerifyDataBatchCount, 4));
}

}  // namespace

}  // namespace syncer
