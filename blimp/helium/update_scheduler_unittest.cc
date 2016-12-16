// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/update_scheduler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/proto/helium.pb.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/mock_objects.h"
#include "blimp/helium/object_sync_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Expectation;
using testing::Return;
using testing::ReturnPointee;
using testing::SaveArg;

namespace blimp {
namespace helium {
namespace {

using MessagePreparedCallback = UpdateScheduler::MessagePreparedCallback;

constexpr HeliumObjectId kFirstObjectId = 24;
constexpr HeliumObjectId kSecondObjectId = 42;
constexpr Revision kFirstRevision = 1;
constexpr Revision kSecondRevision = 2;

// Dummy values and corresponding changesets for those values.
constexpr int32_t kFirstChangesetValue = 5;
const char kFirstChangeset[] = "\x5";
constexpr int32_t kSecondChangesetValue = 6;
const char kSecondChangeset[] = "\x6";

class UpdateSchedulerTest : public HeliumTest {
 public:
  UpdateSchedulerTest() : scheduler_(UpdateScheduler::Create()) {
    SetupObjectSyncStates();
    ConnectAndAcknowledgeObjects();
    SetupTestChangesets();
  }
  ~UpdateSchedulerTest() override = default;

  MOCK_METHOD1(MessagePrepared, void(proto::HeliumMessage));

 protected:
  MessagePreparedCallback CreateTestMessagePreparedCallback() {
    return base::Bind(&UpdateSchedulerTest::MessagePrepared,
                      base::Unretained(this));
  }

  // Helium objects used across tests.
  std::unique_ptr<UpdateScheduler> scheduler_;
  MockStreamPump pump_;
  MockSyncable syncable1_;
  MockSyncable syncable2_;
  std::unique_ptr<ObjectSyncState> state1_;
  std::unique_ptr<ObjectSyncState> state2_;
  std::unique_ptr<TestSyncableChangeset> changeset1_;
  std::unique_ptr<TestSyncableChangeset> changeset2_;

  // Holds the local update callback that |state1_| gives to |syncable1_|.
  base::Closure local_update_callback1_;
  // Holds the local update callback that |state2_| gives to |syncable2_|.
  base::Closure local_update_callback2_;
  // Holds the HeliumMessage sent to |pump_|.
  proto::HeliumMessage sent_message_;

 private:
  void SetupObjectSyncStates() {
    // Create ObjectSyncStates for each object and store the local update
    // callbacks.
    EXPECT_CALL(syncable1_, SetLocalUpdateCallback(_))
        .WillOnce(SaveArg<0>(&local_update_callback1_));
    state1_ = ObjectSyncState::CreateForObject(kFirstObjectId, &syncable1_);

    EXPECT_CALL(syncable2_, SetLocalUpdateCallback(_))
        .WillOnce(SaveArg<0>(&local_update_callback2_));
    state2_ = ObjectSyncState::CreateForObject(kSecondObjectId, &syncable2_);
  }

  void ConnectAndAcknowledgeObjects() {
    // Add the objects to |scheduler_|.
    scheduler_->AddObject(kFirstObjectId, state1_.get());
    scheduler_->AddObject(kSecondObjectId, state2_.get());

    // Connect the |pump_|.
    scheduler_->OnStreamConnected(&pump_);

    // |pump_| asks |scheduler_| for acknowlegement messages for each object. We
    // are able to call PrepareMessage() twice in a row because we know that
    // these are acknowledgement messages and therefore will be made
    // synchronously.
    scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());
    scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());
  }

  void SetupTestChangesets() {
    changeset1_ = base::MakeUnique<TestSyncableChangeset>();
    changeset2_ = base::MakeUnique<TestSyncableChangeset>();
    changeset1_->value.Set(kFirstChangesetValue);
    changeset2_->value.Set(kSecondChangesetValue);
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateSchedulerTest);
};

TEST_F(UpdateSchedulerTest, BasicLocalUpdateFlow) {
  base::Closure prepared_for_changeset_callback;

  EXPECT_CALL(pump_, OnMessageAvailable());
  EXPECT_CALL(syncable1_, GetRevision()).WillRepeatedly(Return(kFirstRevision));
  Expectation prepare_to_create_changeset =
      EXPECT_CALL(syncable1_, PrepareToCreateChangeset(_, _))
          .WillOnce(SaveArg<1>(&prepared_for_changeset_callback));
  EXPECT_CALL(syncable1_, CreateChangesetMock(_))
      .After(prepare_to_create_changeset)
      .WillOnce(Return(changeset1_.release()));
  EXPECT_CALL(*this, MessagePrepared(_)).WillOnce(SaveArg<0>(&sent_message_));

  // |syncable1_| is locally updated.
  local_update_callback1_.Run();

  // Verify that |scheduler_| is ready to send a message with the update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| lets |state1_| know it's ready to create a Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kFirstRevision, sent_message_.end_revision());
  EXPECT_EQ(kFirstChangeset, sent_message_.changeset());

  // Verify that |scheduler_| is no longer ready to send a message.
  EXPECT_FALSE(scheduler_->HasMessageReady());
}

TEST_F(UpdateSchedulerTest, LocalUpdateTwice) {
  base::Closure prepared_for_changeset_callback;
  Revision current_revision = kFirstRevision;

  EXPECT_CALL(pump_, OnMessageAvailable()).Times(2);
  EXPECT_CALL(syncable1_, GetRevision())
      .WillRepeatedly(ReturnPointee(&current_revision));
  EXPECT_CALL(syncable1_, PrepareToCreateChangeset(_, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&prepared_for_changeset_callback));
  EXPECT_CALL(syncable1_, CreateChangesetMock(_))
      .Times(2)
      .WillOnce(Return(changeset1_.release()))
      .WillOnce(Return(changeset2_.release()));
  EXPECT_CALL(*this, MessagePrepared(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&sent_message_));

  // |syncable1_| is locally updated.
  local_update_callback1_.Run();

  // Verify that |scheduler_| is ready to send a message with the update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| lets |state1_| know it's ready to create a Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kFirstRevision, sent_message_.end_revision());
  EXPECT_EQ(kFirstChangeset, sent_message_.changeset());

  // Verify that |scheduler_| no longer has any updates to send.
  EXPECT_FALSE(scheduler_->HasMessageReady());

  // |syncable1_| has another local update.
  local_update_callback1_.Run();

  // Verify that |scheduler_| is ready to send the next update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for the next HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| updates internally to prepare to create the Changeset.
  current_revision = kSecondRevision;

  // |syncable1_| lets |state1_| know it's ready to create the next Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kSecondRevision, sent_message_.end_revision());
  EXPECT_EQ(kSecondChangeset, sent_message_.changeset());
}

TEST_F(UpdateSchedulerTest, LocalUpdateMultipleObjects) {
  base::Closure prepared_for_changeset_callback1;
  base::Closure prepared_for_changeset_callback2;

  EXPECT_CALL(pump_, OnMessageAvailable()).Times(2);
  EXPECT_CALL(syncable1_, GetRevision()).WillRepeatedly(Return(kFirstRevision));
  Expectation prepare_to_create_changeset1 =
      EXPECT_CALL(syncable1_, PrepareToCreateChangeset(_, _))
          .WillOnce(SaveArg<1>(&prepared_for_changeset_callback1));
  EXPECT_CALL(syncable1_, CreateChangesetMock(_))
      .After(prepare_to_create_changeset1)
      .WillOnce(Return(changeset1_.release()));
  EXPECT_CALL(syncable2_, GetRevision())
      .WillRepeatedly(Return(kSecondRevision));
  Expectation prepare_to_create_changeset2 =
      EXPECT_CALL(syncable2_, PrepareToCreateChangeset(_, _))
          .WillOnce(SaveArg<1>(&prepared_for_changeset_callback2));
  EXPECT_CALL(syncable2_, CreateChangesetMock(_))
      .After(prepare_to_create_changeset2)
      .WillOnce(Return(changeset2_.release()));
  EXPECT_CALL(*this, MessagePrepared(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&sent_message_));

  // |syncable1_| and |syncable2_| are locally updated.
  local_update_callback1_.Run();
  local_update_callback2_.Run();

  // Verify that |scheduler_| is ready to send a message with the update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| lets |state1_| know it's ready to create a Changeset.
  prepared_for_changeset_callback1.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kFirstRevision, sent_message_.end_revision());
  EXPECT_EQ(kFirstChangeset, sent_message_.changeset());

  // Verify that |scheduler_| is ready to send the next update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for the next HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable2_| lets |state2_| know it's ready to create a Changeset.
  prepared_for_changeset_callback2.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kSecondObjectId, sent_message_.object_id());
  EXPECT_EQ(kSecondRevision, sent_message_.end_revision());
  EXPECT_EQ(kSecondChangeset, sent_message_.changeset());

  // Verify that |scheduler_| is no longer ready to send a message.
  EXPECT_FALSE(scheduler_->HasMessageReady());
}

TEST_F(UpdateSchedulerTest, LocalUpdateDuringChangesetPreparation) {
  base::Closure prepared_for_changeset_callback;
  Revision current_revision = kFirstRevision;

  EXPECT_CALL(pump_, OnMessageAvailable()).Times(2);
  EXPECT_CALL(syncable1_, GetRevision())
      .WillRepeatedly(ReturnPointee(&current_revision));
  EXPECT_CALL(syncable1_, PrepareToCreateChangeset(_, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&prepared_for_changeset_callback));
  EXPECT_CALL(syncable1_, CreateChangesetMock(_))
      .Times(2)
      .WillOnce(Return(changeset1_.release()))
      .WillOnce(Return(changeset2_.release()));
  EXPECT_CALL(*this, MessagePrepared(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&sent_message_));

  // |syncable1_| is locally updated.
  local_update_callback1_.Run();

  // Verify that |scheduler_| is ready to send a message with the update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| has another local update during preparation.
  local_update_callback1_.Run();

  // |syncable1_| lets |state1_| know it's ready to create a Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kFirstRevision, sent_message_.end_revision());
  EXPECT_EQ(kFirstChangeset, sent_message_.changeset());

  // Verify that |scheduler_| is ready to send the next update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for the next HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| updates internally to prepare to create the Changeset.
  current_revision = kSecondRevision;

  // |syncable1_| lets |state1_| know it's ready to create the next Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kSecondRevision, sent_message_.end_revision());
  EXPECT_EQ(kSecondChangeset, sent_message_.changeset());
}

TEST_F(UpdateSchedulerTest, BasicRemoteUpdateFlow) {
  TestSyncableChangeset validated_changeset;
  TestSyncableChangeset applied_changeset;

  Expectation changeset_validation =
      EXPECT_CALL(syncable1_, ValidateChangeset(_))
          .WillOnce(DoAll(SaveArg<0>(&validated_changeset), Return(true)));
  EXPECT_CALL(syncable1_, ApplyChangeset(_))
      .After(changeset_validation)
      .WillOnce(SaveArg<0>(&applied_changeset));
  EXPECT_CALL(pump_, OnMessageAvailable());
  EXPECT_CALL(*this, MessagePrepared(_)).WillOnce(SaveArg<0>(&sent_message_));

  // |state1_| receives a Changeset from the SyncManager.
  state1_->OnChangesetReceived(kFirstRevision, kFirstChangeset);

  // Verify that |syncable1_| validates and applies the given Changeset.
  EXPECT_EQ(kFirstChangesetValue, validated_changeset.value());
  EXPECT_EQ(kFirstChangesetValue, applied_changeset.value());

  // Verify that |scheduler_| is ready to send a message to acknowledge the
  // Changeset.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // Verify that |pump_| receives the correct HeliumMessage.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kFirstRevision, sent_message_.ack_revision());
}

TEST_F(UpdateSchedulerTest, LocalUpdateAndAcknowledgementOneMessage) {
  base::Closure prepared_for_changeset_callback;
  TestSyncableChangeset validated_changeset;
  TestSyncableChangeset applied_changeset;

  Expectation changeset_validation =
      EXPECT_CALL(syncable1_, ValidateChangeset(_))
          .WillOnce(DoAll(SaveArg<0>(&validated_changeset), Return(true)));
  EXPECT_CALL(syncable1_, ApplyChangeset(_))
      .After(changeset_validation)
      .WillOnce(SaveArg<0>(&applied_changeset));
  EXPECT_CALL(pump_, OnMessageAvailable()).Times(2);
  EXPECT_CALL(syncable1_, GetRevision())
      .WillRepeatedly(Return(kSecondRevision));
  Expectation prepare_to_create_changeset =
      EXPECT_CALL(syncable1_, PrepareToCreateChangeset(_, _))
          .WillOnce(SaveArg<1>(&prepared_for_changeset_callback));
  EXPECT_CALL(syncable1_, CreateChangesetMock(_))
      .After(prepare_to_create_changeset)
      .WillOnce(Return(changeset1_.release()));
  EXPECT_CALL(*this, MessagePrepared(_)).WillOnce(SaveArg<0>(&sent_message_));

  // |state1_| receives a Changeset from the SyncManager.
  state1_->OnChangesetReceived(kFirstRevision, kFirstChangeset);

  // Verify that |syncable1_| validates and applies the given Changeset.
  EXPECT_EQ(kFirstChangesetValue, validated_changeset.value());
  EXPECT_EQ(kFirstChangesetValue, applied_changeset.value());

  // |syncable1_| is locally updated.
  local_update_callback1_.Run();

  // Verify that |scheduler_| is ready to send a message with the update.
  EXPECT_TRUE(scheduler_->HasMessageReady());

  // |pump_| asks |scheduler_| for a HeliumMessage.
  scheduler_->PrepareMessage(CreateTestMessagePreparedCallback());

  // |syncable1_| lets |state1_| know it's ready to create a Changeset.
  prepared_for_changeset_callback.Run();

  // Verify that |pump_| receives the correct HeliumMessage, with both the local
  // update and the acknowledgement of the received Changeset.
  EXPECT_EQ(kFirstObjectId, sent_message_.object_id());
  EXPECT_EQ(kSecondRevision, sent_message_.end_revision());
  EXPECT_EQ(kFirstChangeset, sent_message_.changeset());
  EXPECT_EQ(kFirstRevision, sent_message_.ack_revision());

  // Verify that |scheduler_| is no longer ready to send a message.
  EXPECT_FALSE(scheduler_->HasMessageReady());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
