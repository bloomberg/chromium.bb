// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service_impl.h"

#include <utility>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/user_events/user_event_sync_bridge.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;
using testing::_;

namespace syncer {

namespace {

std::unique_ptr<UserEventSpecifics> Event() {
  return std::make_unique<UserEventSpecifics>();
}

std::unique_ptr<UserEventSpecifics> AsTest(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_test_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsDetection(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_language_detection_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsTrial(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_field_trial_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> WithNav(
    std::unique_ptr<UserEventSpecifics> specifics,
    int64_t navigation_id = 1) {
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

class TestGlobalIdMapper : public GlobalIdMapper {
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {}
  int64_t GetLatestGlobalId(int64_t global_id) override { return global_id; }
};

class UserEventServiceImplTest : public testing::Test {
 protected:
  UserEventServiceImplTest() {
    sync_service_.SetPreferredDataTypes(
        {HISTORY_DELETE_DIRECTIVES, USER_EVENTS});
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_processor_, TrackedAccountId())
        .WillByDefault(testing::Return("account_id"));
  }

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    return std::make_unique<UserEventSyncBridge>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor(), &mapper_);
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  MockModelTypeChangeProcessor* mock_processor() { return &mock_processor_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  TestGlobalIdMapper mapper_;
};

TEST_F(UserEventServiceImplTest, MightRecordEvents) {
  // All conditions are met, might record.
  EXPECT_TRUE(UserEventServiceImpl::MightRecordEvents(false, sync_service()));
  // No sync service, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(false, nullptr));
  // Off the record, will not record.
  EXPECT_FALSE(UserEventServiceImpl::MightRecordEvents(true, sync_service()));
}

TEST_F(UserEventServiceImplTest, ShouldRecord) {
  UserEventServiceImpl service(sync_service(), MakeBridge());
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest,
       ShouldOnlyRecordEventsWithoutNavIdWhenHistorySyncIsDisabled) {
  sync_service()->SetPreferredDataTypes({USER_EVENTS});
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Only record events without navigation ids when history sync is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenPassphraseIsUsed) {
  sync_service()->SetIsUsingSecondaryPassphrase(true);
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Do not record events when a passphrase is used.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenEngineIsNotInitialized) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Do not record events when the engine is off.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordEmptyEvents) {
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // All untyped events should always be ignored.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(Event());
  service.RecordUserEvent(WithNav(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordHasNavigationId) {
  UserEventServiceImpl service(sync_service(), MakeBridge());

  // Verify logic for types that might or might not have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTest(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(WithNav(AsTest(Event())));

  // Verify logic for types that must have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsDetection(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(WithNav(AsDetection(Event())));

  // Verify logic for types that cannot have a navigation id.
  EXPECT_CALL(*mock_processor(), Put(_, _, _));
  service.RecordUserEvent(AsTrial(Event()));
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(WithNav(AsTrial(Event())));
}

TEST_F(UserEventServiceImplTest, SessionIdIsDifferent) {
  std::vector<int64_t> put_session_ids;
  ON_CALL(*mock_processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         const std::unique_ptr<EntityData> entity_data,
                         MetadataChangeList* metadata_change_list) {
        put_session_ids.push_back(
            entity_data->specifics.user_event().session_id());
      });

  UserEventServiceImpl service1(sync_service(), MakeBridge());
  service1.RecordUserEvent(AsTest(Event()));

  UserEventServiceImpl service2(sync_service(), MakeBridge());
  service2.RecordUserEvent(AsTest(Event()));

  ASSERT_EQ(2U, put_session_ids.size());
  EXPECT_NE(put_session_ids[0], put_session_ids[1]);
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenEventsDatatypeIsDisabled) {
  sync_service()->SetPreferredDataTypes({HISTORY_DELETE_DIRECTIVES});
  UserEventServiceImpl service(sync_service(), MakeBridge());
  // USER_EVENTS type is disabled, thus, they should not be recorded.
  EXPECT_CALL(*mock_processor(), Put(_, _, _)).Times(0);
  service.RecordUserEvent(AsTest(Event()));
}

}  // namespace

}  // namespace syncer
