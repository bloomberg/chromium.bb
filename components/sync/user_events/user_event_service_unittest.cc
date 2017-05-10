// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service.h"

#include "base/memory/ptr_util.h"
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
  TestSyncService(bool is_engine_initialized, ModelTypeSet preferred_data_types)
      : is_engine_initialized_(is_engine_initialized),
        preferred_data_types_(preferred_data_types) {}

  bool IsEngineInitialized() const override { return is_engine_initialized_; }

  ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }

 private:
  bool is_engine_initialized_;
  ModelTypeSet preferred_data_types_;
};

class UserEventServiceTest : public testing::Test {
 protected:
  UserEventServiceTest() {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(switches::kSyncUserEvents);
  }

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    return base::MakeUnique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        RecordingModelTypeChangeProcessor::FactoryForBridgeTest(&processor_));
  }

  const RecordingModelTypeChangeProcessor& processor() { return *processor_; }

  void DisableUserEvents() {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->Init();
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  RecordingModelTypeChangeProcessor* processor_;
  base::MessageLoop message_loop_;
};

TEST_F(UserEventServiceTest, ShouldNotRecordNoSync) {
  UserEventService service(nullptr, MakeBridge());
  service.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceTest, ShouldNotRecordFeatureIsDisabled) {
  DisableUserEvents();
  TestSyncService sync_service(false, ModelTypeSet(HISTORY_DELETE_DIRECTIVES));
  UserEventService service(&sync_service, MakeBridge());
  service.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceTest, ShouldNotRecordNoHistory) {
  TestSyncService sync_service(true, ModelTypeSet());
  UserEventService service(&sync_service, MakeBridge());
  service.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceTest, ShouldNotRecordEngineOff) {
  TestSyncService sync_service(false, ModelTypeSet(HISTORY_DELETE_DIRECTIVES));
  UserEventService service(&sync_service, MakeBridge());
  service.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  EXPECT_EQ(0u, processor().put_multimap().size());
}

TEST_F(UserEventServiceTest, ShouldRecord) {
  TestSyncService sync_service(true, ModelTypeSet(HISTORY_DELETE_DIRECTIVES));
  UserEventService service(&sync_service, MakeBridge());
  service.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  EXPECT_EQ(1u, processor().put_multimap().size());
}

TEST_F(UserEventServiceTest, SessionIdIsDifferent) {
  TestSyncService sync_service(true, ModelTypeSet(HISTORY_DELETE_DIRECTIVES));

  UserEventService service1(&sync_service, MakeBridge());
  service1.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  ASSERT_EQ(1u, processor().put_multimap().size());
  auto put1 = processor().put_multimap().begin();
  int64_t session_id1 = put1->second->specifics.user_event().session_id();

  UserEventService service2(&sync_service, MakeBridge());
  service2.RecordUserEvent(base::MakeUnique<UserEventSpecifics>());
  // The object processor() points to has changed to be |service2|'s processor.
  ASSERT_EQ(1u, processor().put_multimap().size());
  auto put2 = processor().put_multimap().begin();
  int64_t session_id2 = put2->second->specifics.user_event().session_id();

  EXPECT_NE(session_id1, session_id2);
}

}  // namespace

}  // namespace syncer
