// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model/recording_model_type_change_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/user_events/user_event_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

namespace {

class TestSyncService : public FakeSyncService {
 public:
  TestSyncService(bool is_engine_initialized,
                  bool is_using_secondary_passphrase,
                  ModelTypeSet preferred_data_types)
      : is_engine_initialized_(is_engine_initialized),
        is_using_secondary_passphrase_(is_using_secondary_passphrase),
        preferred_data_types_(preferred_data_types) {}

  bool IsEngineInitialized() const override { return is_engine_initialized_; }
  bool IsUsingSecondaryPassphrase() const override {
    return is_using_secondary_passphrase_;
  }

  ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }

 private:
  bool is_engine_initialized_;
  bool is_using_secondary_passphrase_;
  ModelTypeSet preferred_data_types_;
};

class TestGlobalIdMapper : public GlobalIdMapper {
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {}
  int64_t GetLatestGlobalId(int64_t global_id) override { return global_id; }
};

class UserEventServiceImplTest : public testing::Test {
 protected:
  UserEventServiceImplTest()
      : sync_service_(true, false, {HISTORY_DELETE_DIRECTIVES}) {}

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    return std::make_unique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        RecordingModelTypeChangeProcessor::FactoryForBridgeTest(&processor_),
        &mapper_);
  }

  TestSyncService* sync_service() { return &sync_service_; }
  const RecordingModelTypeChangeProcessor& processor() { return *processor_; }

 private:
  TestSyncService sync_service_;
  RecordingModelTypeChangeProcessor* processor_;
  TestGlobalIdMapper mapper_;
  base::MessageLoop message_loop_;
};

TEST_F(UserEventServiceImplTest, MightRecordEventsFeatureEnabled) {
  // All conditions are met, might record.
  EXPECT_TRUE(UserEventServiceImpl::MightRecordEvents(false, sync_service()));
  // No sync service, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(false, nullptr));
  // Off the record, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(true, sync_service()));
}

TEST_F(UserEventServiceImplTest, MightRecordEventsFeatureDisabled) {
  // Will not record because the default on feature is overridden.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(switches::kSyncUserEvents);
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(false, sync_service()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordNoHistory) {
  TestSyncService no_history_sync_service(true, false, ModelTypeSet());
  UserEventServiceImpl service(&no_history_sync_service, MakeBridge());
  service.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordPassphrase) {
  TestSyncService passphrase_sync_service(true, true,
                                          {HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(&passphrase_sync_service, MakeBridge());
  service.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordEngineOff) {
  TestSyncService engine_not_initialized_sync_service(
      false, false, {HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(&engine_not_initialized_sync_service,
                               MakeBridge());
  service.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceImplTest, ShouldRecord) {
  UserEventServiceImpl service(sync_service(), MakeBridge());
  service.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  EXPECT_EQ(1u, processor().put_multimap().size());
}

TEST_F(UserEventServiceImplTest, SessionIdIsDifferent) {
  UserEventServiceImpl service1(sync_service(), MakeBridge());
  service1.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  ASSERT_EQ(1u, processor().put_multimap().size());
  auto put1 = processor().put_multimap().begin();
  int64_t session_id1 = put1->second->specifics.user_event().session_id();

  UserEventServiceImpl service2(sync_service(), MakeBridge());
  service2.RecordUserEvent(std::make_unique<UserEventSpecifics>());
  // The object processor() points to has changed to be |service2|'s processor.
  ASSERT_EQ(1u, processor().put_multimap().size());
  auto put2 = processor().put_multimap().begin();
  int64_t session_id2 = put2->second->specifics.user_event().session_id();

  EXPECT_NE(session_id1, session_id2);
}

}  // namespace

}  // namespace syncer
