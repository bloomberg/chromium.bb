// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_model_type_processor.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/nigori/nigori_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::NotNull;

const char kNigoriNonUniqueName[] = "nigori";
const char kNigoriServerId[] = "nigori_server_id";
const char kCacheGuid[] = "generated_id";

// |*arg| must be of type base::Optional<EntityData>.
MATCHER_P(OptionalEntityDataHasDecryptorTokenKeyName, expected_key_name, "") {
  return arg->specifics.nigori().keystore_decryptor_token().key_name() ==
         expected_key_name;
}

void CaptureCommitRequest(CommitRequestDataList* dst,
                          CommitRequestDataList&& src) {
  *dst = std::move(src);
}

sync_pb::ModelTypeState CreateDummyModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_cache_guid(kCacheGuid);
  model_type_state.set_initial_sync_done(true);
  return model_type_state;
}

// Creates a dummy Nigori UpdateResponseData that has the keystore decryptor
// token key name set.
std::unique_ptr<syncer::UpdateResponseData> CreateDummyNigoriUpdateResponseData(
    const std::string keystore_decryptor_token_key_name,
    int response_version) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->id = kNigoriServerId;
  sync_pb::NigoriSpecifics* nigori_specifics =
      entity_data->specifics.mutable_nigori();
  nigori_specifics->mutable_keystore_decryptor_token()->set_key_name(
      keystore_decryptor_token_key_name);
  entity_data->non_unique_name = kNigoriNonUniqueName;

  auto response_data = std::make_unique<syncer::UpdateResponseData>();
  response_data->entity = std::move(entity_data);
  response_data->response_version = response_version;
  return response_data;
}

class MockNigoriSyncBridge : public NigoriSyncBridge {
 public:
  MockNigoriSyncBridge() = default;
  ~MockNigoriSyncBridge() = default;

  MOCK_METHOD1(
      MergeSyncData,
      base::Optional<ModelError>(const base::Optional<EntityData>& data));
  MOCK_METHOD1(
      ApplySyncChanges,
      base::Optional<ModelError>(const base::Optional<EntityData>& data));
  MOCK_METHOD0(GetData, std::unique_ptr<EntityData>());
  MOCK_METHOD2(ResolveConflict,
               ConflictResolution(const EntityData& local_data,
                                  const EntityData& remote_data));
  MOCK_METHOD0(ApplyDisableSyncChanges, void());
};

class MockCommitQueue : public CommitQueue {
 public:
  MockCommitQueue() = default;
  ~MockCommitQueue() = default;

  MOCK_METHOD0(NudgeForCommit, void());
};

class NigoriModelTypeProcessorTest : public testing::Test {
 public:
  NigoriModelTypeProcessorTest() {
    mock_commit_queue_ = std::make_unique<testing::NiceMock<MockCommitQueue>>();
    mock_commit_queue_ptr_ = mock_commit_queue_.get();
  }

  void SimulateModelReadyToSync(bool initial_sync_done, int server_version) {
    NigoriMetadataBatch nigori_metadata_batch;
    nigori_metadata_batch.model_type_state.set_initial_sync_done(
        initial_sync_done);
    nigori_metadata_batch.entity_metadata = sync_pb::EntityMetadata();
    nigori_metadata_batch.entity_metadata->set_creation_time(
        TimeToProtoTime(base::Time::Now()));
    nigori_metadata_batch.entity_metadata->set_sequence_number(0);
    nigori_metadata_batch.entity_metadata->set_acked_sequence_number(0);
    nigori_metadata_batch.entity_metadata->set_server_version(server_version);
    processor_.ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));
  }

  void SimulateModelReadyToSync(bool initial_sync_done) {
    SimulateModelReadyToSync(initial_sync_done, /*server_version=*/1);
  }

  void SimulateConnectSync() {
    processor_.ConnectSync(std::move(mock_commit_queue_));
  }

  MockNigoriSyncBridge* mock_nigori_sync_bridge() {
    return &mock_nigori_sync_bridge_;
  }

  MockCommitQueue* mock_commit_queue() { return mock_commit_queue_ptr_; }

  NigoriModelTypeProcessor* processor() { return &processor_; }

 private:
  // This sets SequencedTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::test::ScopedTaskEnvironment task_environment_;

  testing::NiceMock<MockNigoriSyncBridge> mock_nigori_sync_bridge_;
  std::unique_ptr<testing::NiceMock<MockCommitQueue>> mock_commit_queue_;
  MockCommitQueue* mock_commit_queue_ptr_;
  NigoriModelTypeProcessor processor_;
};

TEST_F(NigoriModelTypeProcessorTest,
       ShouldTrackTheMetadataWhenInitialSyncDone) {
  // Build a model type state with a specific cache guid.
  const std::string kCacheGuid = "cache_guid";
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);
  model_type_state.set_cache_guid(kCacheGuid);

  // Build entity metadata with a specific sequence number.
  const int kSequenceNumber = 100;
  sync_pb::EntityMetadata entity_metadata;
  entity_metadata.set_sequence_number(kSequenceNumber);
  entity_metadata.set_creation_time(TimeToProtoTime(base::Time::Now()));

  NigoriMetadataBatch nigori_metadata_batch;
  nigori_metadata_batch.model_type_state = model_type_state;
  nigori_metadata_batch.entity_metadata = entity_metadata;

  processor()->ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));

  // The model type state and the metadata should have been stored in the
  // processor.
  NigoriMetadataBatch processor_metadata_batch = processor()->GetMetadata();
  EXPECT_THAT(processor_metadata_batch.model_type_state.cache_guid(),
              Eq(kCacheGuid));
  ASSERT_TRUE(processor_metadata_batch.entity_metadata);
  EXPECT_THAT(processor_metadata_batch.entity_metadata->sequence_number(),
              Eq(kSequenceNumber));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldIncrementSequenceNumberWhenPut) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  base::Optional<sync_pb::EntityMetadata> entity_metadata1 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  base::Optional<sync_pb::EntityMetadata> entity_metadata2 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  EXPECT_THAT(entity_metadata2->sequence_number(),
              Eq(entity_metadata1->sequence_number() + 1));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldGetEmptyLocalChanges) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());
}

TEST_F(NigoriModelTypeProcessorTest, ShouldGetLocalChangesWhenPut) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));
  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  ASSERT_EQ(1U, commit_request.size());
  EXPECT_EQ(kNigoriNonUniqueName, commit_request[0]->entity->non_unique_name);
}

TEST_F(NigoriModelTypeProcessorTest,
       ShouldNudgeForCommitUponConnectSyncIfReadyToSyncAndLocalChanges) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  SimulateConnectSync();
}

TEST_F(NigoriModelTypeProcessorTest, ShouldNudgeForCommitUponPutIfReadyToSync) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);
  SimulateConnectSync();

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  processor()->Put(std::move(entity_data));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldInvokeSyncStartCallback) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true);

  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  request.cache_guid = kCacheGuid;

  base::MockCallback<ModelTypeControllerDelegate::StartCallback> start_callback;
  std::unique_ptr<DataTypeActivationResponse> captured_response;
  EXPECT_CALL(start_callback, Run)
      .WillOnce(testing::Invoke(
          [&captured_response](
              std::unique_ptr<DataTypeActivationResponse> response) {
            captured_response = std::move(response);
          }));
  processor()->OnSyncStarting(request, start_callback.Get());
  ASSERT_THAT(captured_response, NotNull());
  EXPECT_EQ(kCacheGuid, captured_response->model_type_state.cache_guid());

  // Test that the |processor()| has been set in the activation response.
  ASSERT_FALSE(processor()->IsConnectedForTest());
  captured_response->type_processor->ConnectSync(
      std::make_unique<testing::NiceMock<MockCommitQueue>>());
  // RunUntilIdle() is needed because ModelTypeProcessorProxy() is used and it
  // internally does posting of tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(processor()->IsConnectedForTest());
}

TEST_F(NigoriModelTypeProcessorTest, ShouldMergeSyncData) {
  SimulateModelReadyToSync(/*initial_sync_done=*/false);

  const std::string kDecryptorTokenKeyName = "key_name";
  UpdateResponseDataList updates;
  updates.push_back(CreateDummyNigoriUpdateResponseData(kDecryptorTokenKeyName,
                                                        /*server_version=*/1));

  EXPECT_CALL(*mock_nigori_sync_bridge(),
              MergeSyncData(OptionalEntityDataHasDecryptorTokenKeyName(
                  kDecryptorTokenKeyName)));

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldApplySyncChanges) {
  SimulateModelReadyToSync(/*initial_sync_done=*/true, /*server_version=*/1);

  const std::string kDecryptorTokenKeyName = "key_name";
  UpdateResponseDataList updates;
  updates.push_back(CreateDummyNigoriUpdateResponseData(kDecryptorTokenKeyName,
                                                        /*server_version=*/2));

  EXPECT_CALL(*mock_nigori_sync_bridge(),
              ApplySyncChanges(OptionalEntityDataHasDecryptorTokenKeyName(
                  kDecryptorTokenKeyName)));

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldApplySyncChangesWhenEmptyUpdates) {
  const int kServerVersion = 1;
  SimulateModelReadyToSync(/*initial_sync_done=*/true, kServerVersion);

  // ApplySyncChanges() should still be called to trigger persistence of the
  // metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(), ApplySyncChanges(Eq(base::nullopt)));

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                UpdateResponseDataList());
}

TEST_F(NigoriModelTypeProcessorTest, ShouldApplySyncChangesWhenReflection) {
  const int kServerVersion = 1;
  SimulateModelReadyToSync(/*initial_sync_done=*/true, kServerVersion);

  UpdateResponseDataList updates;
  updates.push_back(CreateDummyNigoriUpdateResponseData(
      /*keystore_decryptor_token_key_name=*/"key_name", kServerVersion));

  // ApplySyncChanges() should still be called to trigger persistence of the
  // metadata.
  EXPECT_CALL(*mock_nigori_sync_bridge(), ApplySyncChanges(Eq(base::nullopt)));

  processor()->OnUpdateReceived(CreateDummyModelTypeState(),
                                std::move(updates));
}

}  // namespace

}  // namespace syncer
