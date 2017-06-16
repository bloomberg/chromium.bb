// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/shared_model_type_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/model/fake_model_type_sync_bridge.h"
#include "components/sync/test/engine/mock_model_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntitySpecifics;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;

namespace syncer {

namespace {

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kKey4[] = "key4";
const char kKey5[] = "key5";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";
const std::string kHash1(FakeModelTypeSyncBridge::TagHashFromKey(kKey1));
const std::string kHash2(FakeModelTypeSyncBridge::TagHashFromKey(kKey2));
const std::string kHash3(FakeModelTypeSyncBridge::TagHashFromKey(kKey3));
const std::string kHash4(FakeModelTypeSyncBridge::TagHashFromKey(kKey4));
const std::string kHash5(FakeModelTypeSyncBridge::TagHashFromKey(kKey5));

// Typically used for verification after a delete. The specifics given to the
// worker/processor will not have been initialized and thus empty.
const EntitySpecifics kEmptySpecifics;

EntitySpecifics GenerateSpecifics(const std::string& key,
                                  const std::string& value) {
  return FakeModelTypeSyncBridge::GenerateSpecifics(key, value);
}

std::unique_ptr<EntityData> GenerateEntityData(const std::string& key,
                                               const std::string& value) {
  return FakeModelTypeSyncBridge::GenerateEntityData(key, value);
}

std::unique_ptr<ModelTypeChangeProcessor>
CreateProcessor(bool commit_only, ModelType type, ModelTypeSyncBridge* bridge) {
  return base::MakeUnique<SharedModelTypeProcessor>(
      type, bridge, base::RepeatingClosure(), commit_only);
}

class TestModelTypeSyncBridge : public FakeModelTypeSyncBridge {
 public:
  explicit TestModelTypeSyncBridge(bool commit_only)
      : FakeModelTypeSyncBridge(base::Bind(&CreateProcessor, commit_only)) {}

  TestModelTypeSyncBridge(std::unique_ptr<TestModelTypeSyncBridge> other,
                          bool commit_only)
      : TestModelTypeSyncBridge(commit_only) {
    std::swap(db_, other->db_);
  }

  ~TestModelTypeSyncBridge() override {
    EXPECT_FALSE(synchronous_data_callback_);
    EXPECT_FALSE(data_callback_);
  }

  std::string GetStorageKey(const EntityData& entity_data) override {
    get_storage_key_call_count_++;
    return FakeModelTypeSyncBridge::GetStorageKey(entity_data);
  }

  void OnPendingCommitDataLoaded() {
    ASSERT_TRUE(data_callback_);
    data_callback_.Run();
    data_callback_.Reset();
  }

  void InitializeToReadyState() {
    if (!data_callback_.is_null()) {
      OnPendingCommitDataLoaded();
    }
  }

  void SetInitialSyncDone(bool is_done) {
    ModelTypeState model_type_state(db().model_type_state());
    model_type_state.set_initial_sync_done(is_done);
    db_->set_model_type_state(model_type_state);
  }

  // Expect a GetData call in the future and return its data immediately.
  void ExpectSynchronousDataCallback() { synchronous_data_callback_ = true; }

  int merge_call_count() const { return merge_call_count_; }
  int apply_call_count() const { return apply_call_count_; }
  int get_storage_key_call_count() const { return get_storage_key_call_count_; }

  // FakeModelTypeSyncBridge overrides.

  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override {
    merge_call_count_++;
    return FakeModelTypeSyncBridge::MergeSyncData(
        std::move(metadata_change_list), entity_data);
  }
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override {
    apply_call_count_++;
    return FakeModelTypeSyncBridge::ApplySyncChanges(
        std::move(metadata_change_list), entity_changes);
  }

  void GetData(StorageKeyList keys, DataCallback callback) override {
    if (synchronous_data_callback_) {
      synchronous_data_callback_ = false;
      FakeModelTypeSyncBridge::GetData(keys, callback);
    } else {
      FakeModelTypeSyncBridge::GetData(
          keys, base::Bind(&TestModelTypeSyncBridge::CaptureDataCallback,
                           base::Unretained(this), callback));
    }
  }

 private:
  void CaptureDataCallback(DataCallback callback,
                           std::unique_ptr<DataBatch> data) {
    EXPECT_FALSE(data_callback_);
    data_callback_ = base::Bind(callback, base::Passed(std::move(data)));
  }

  // The number of times MergeSyncData has been called.
  int merge_call_count_ = 0;
  int apply_call_count_ = 0;
  int get_storage_key_call_count_ = 0;

  // Stores the data callback between GetData() and OnPendingCommitDataLoaded().
  base::Closure data_callback_;

  // Whether to return GetData results synchronously. Overrides the default
  // callback capture behavior if set to true.
  bool synchronous_data_callback_ = false;
};

}  // namespace

// Tests the various functionality of SharedModelTypeProcessor.
//
// The processor sits between the bridge (implemented by this test class) and
// the worker, which is represented by a MockModelTypeWorker. This test suite
// exercises the initialization flows (whether initial sync is done, performing
// the initial merge, etc) as well as normal functionality:
//
// - Initialization before the initial sync and merge correctly performs a merge
//   and initializes the metadata in storage.
// - Initialization after the initial sync correctly loads metadata and queues
//   any pending commits.
// - Put and Delete calls from the bridge result in the correct metadata in
//   storage and the correct commit requests on the worker side.
// - Updates and commit responses from the worker correctly affect data and
//   metadata in storage on the bridge side.
class SharedModelTypeProcessorTest : public ::testing::Test {
 public:
  SharedModelTypeProcessorTest()
      : bridge_(base::MakeUnique<TestModelTypeSyncBridge>(false)) {}

  ~SharedModelTypeProcessorTest() override { CheckPostConditions(); }

  void InitializeToMetadataLoaded() {
    bridge()->SetInitialSyncDone(true);
    ModelReadyToSync();
  }

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState() {
    InitializeToMetadataLoaded();
    bridge()->InitializeToReadyState();
    OnSyncStarting();
  }

  void ModelReadyToSync() {
    type_processor()->ModelReadyToSync(db().CreateMetadataBatch());
  }

  void OnPendingCommitDataLoaded() { bridge()->OnPendingCommitDataLoaded(); }

  void OnSyncStarting() {
    type_processor()->OnSyncStarting(
        base::BindRepeating(&SharedModelTypeProcessorTest::ErrorReceived,
                            base::Unretained(this)),
        base::Bind(&SharedModelTypeProcessorTest::OnReadyToConnect,
                   base::Unretained(this)));
  }

  void DisconnectSync() {
    type_processor()->DisconnectSync();
    worker_ = nullptr;
  }

  // Writes data for |key| and simulates a commit response for it.
  EntitySpecifics WriteItemAndAck(const std::string& key,
                                  const std::string& value) {
    EntitySpecifics specifics = bridge()->WriteItem(key, value);
    worker()->VerifyPendingCommits(
        {FakeModelTypeSyncBridge::TagHashFromKey(key)});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return specifics;
  }

  void ResetState(bool keep_db, bool commit_only = false) {
    bridge_ = keep_db ? base::MakeUnique<TestModelTypeSyncBridge>(
                            std::move(bridge_), commit_only)
                      : base::MakeUnique<TestModelTypeSyncBridge>(commit_only);
    worker_ = nullptr;
    CheckPostConditions();
  }

  // Wipes existing DB and simulates a pending update of a server-known item.
  EntitySpecifics ResetStateWriteItem(const std::string& name,
                                      const std::string& value) {
    ResetState(false);
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, "acked-value");
    EntitySpecifics specifics = bridge()->WriteItem(name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    ResetState(true);
    return specifics;
  }

  // Wipes existing DB and simulates a pending deletion of a server-known item.
  void ResetStateDeleteItem(const std::string& name, const std::string& value) {
    ResetState(false);
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    bridge()->DeleteItem(name);
    EXPECT_EQ(1U, ProcessorEntityCount());
    ResetState(true);
  }

  // Return the number of entities the processor has metadata for.
  size_t ProcessorEntityCount() const {
    return type_processor()->entities_.size();
  }

  // Expect to receive an error from the processor.
  void ExpectError() {
    EXPECT_FALSE(expect_error_);
    expect_error_ = true;
  }

  TestModelTypeSyncBridge* bridge() const { return bridge_.get(); }

  const FakeModelTypeSyncBridge::Store& db() const { return bridge()->db(); }

  MockModelTypeWorker* worker() const { return worker_; }

  SharedModelTypeProcessor* type_processor() const {
    return static_cast<SharedModelTypeProcessor*>(bridge()->change_processor());
  }

 private:
  void CheckPostConditions() { EXPECT_FALSE(expect_error_); }

  void OnReadyToConnect(std::unique_ptr<ActivationContext> context) {
    std::unique_ptr<MockModelTypeWorker> worker(
        new MockModelTypeWorker(context->model_type_state, type_processor()));
    // Keep an unsafe pointer to the commit queue the processor will use.
    worker_ = worker.get();
    // The context contains a proxy to the processor, but this call is
    // side-stepping that completely and connecting directly to the real
    // processor, since these tests are single-threaded and don't need proxies.
    type_processor()->ConnectSync(std::move(worker));
  }

  void ErrorReceived(const ModelError& error) {
    EXPECT_TRUE(expect_error_);
    expect_error_ = false;
  }

  std::unique_ptr<TestModelTypeSyncBridge> bridge_;

  // This sets ThreadTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::MessageLoop sync_loop_;

  // The current mock queue, which is owned by |type_processor()|.
  MockModelTypeWorker* worker_;

  // Whether to expect an error from the processor.
  bool expect_error_ = false;
};

// Test that an initial sync handles local and remote items properly.
TEST_F(SharedModelTypeProcessorTest, InitialSync) {
  ModelReadyToSync();
  OnSyncStarting();

  // Local write before initial sync.
  bridge()->WriteItem(kKey1, kValue1);

  // Has data, but no metadata, entity in the processor, or commit request.
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with one server item.
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Now have data and metadata for both items, as well as a commit request for
  // the local item.
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(0, db().GetMetadata(kKey2).sequence_number());
  worker()->VerifyPendingCommits({kHash1});
}

// Test that an initial sync filters out tombstones in the processor.
TEST_F(SharedModelTypeProcessorTest, InitialSyncWithTombstone) {
  ModelReadyToSync();
  OnSyncStarting();

  EXPECT_EQ(0, bridge()->merge_call_count());
  // Initial sync with a tombstone. The fake bridge checks that it doesn't get
  // any tombstones in its MergeSyncData function.
  worker()->TombstoneFromServer(kHash1);
  EXPECT_EQ(1, bridge()->merge_call_count());

  // Should still have no data, metadata, or commit requests.
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Test that subsequent starts don't call MergeSyncData.
TEST_F(SharedModelTypeProcessorTest, NonInitialSync) {
  // This sets initial_sync_done to true.
  InitializeToMetadataLoaded();

  // Write an item before sync connects.
  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  // Check that data coming from sync is treated as a normal GetUpdates.
  OnSyncStarting();
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(0, bridge()->merge_call_count());
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
}

// Test that an error during the merge is propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, MergeError) {
  ModelReadyToSync();
  OnSyncStarting();

  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateFromServer();
}

// Test that errors before it's called are passed to |start_callback| correctly.
TEST_F(SharedModelTypeProcessorTest, StartErrors) {
  type_processor()->ReportError(FROM_HERE, "boom");
  ExpectError();
  OnSyncStarting();

  // Test OnSyncStarting happening first.
  ResetState(false);
  OnSyncStarting();
  ExpectError();
  type_processor()->ReportError(FROM_HERE, "boom");

  // Test an error loading pending data.
  ResetStateWriteItem(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  InitializeToMetadataLoaded();
  ExpectError();
  OnSyncStarting();

  // Test an error prior to metadata load.
  ResetState(false);
  type_processor()->ReportError(FROM_HERE, "boom");
  ExpectError();
  OnSyncStarting();
  ModelReadyToSync();

  // Test an error prior to pending data load.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  type_processor()->ReportError(FROM_HERE, "boom");
  ExpectError();
  OnSyncStarting();
  OnPendingCommitDataLoaded();
}

// This test covers race conditions during loading pending data. All cases
// start with no processor and one acked (committed to the server) item with a
// pending commit. There are three different events that can occur in any order
// once metadata is loaded:
//
// - Pending commit data is loaded.
// - Sync gets connected.
// - Optionally, a put or delete happens to the item.
//
// This results in 2 + 12 = 14 orderings of the events.
TEST_F(SharedModelTypeProcessorTest, LoadPendingCommit) {
  // Data, connect.
  EntitySpecifics specifics1 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics1);

  // Connect, data.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics2);

  // Data, connect, put.
  EntitySpecifics specifics3 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EntitySpecifics specifics4 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics3);
  worker()->VerifyNthPendingCommit(1, kHash1, specifics4);

  // Data, put, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  EntitySpecifics specifics5 = bridge()->WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics5);

  // Connect, data, put.
  EntitySpecifics specifics6 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnPendingCommitDataLoaded();
  EntitySpecifics specifics7 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics6);
  worker()->VerifyNthPendingCommit(1, kHash1, specifics7);

  // Connect, put, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EntitySpecifics specifics8 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics8);

  // Put, data, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics9 = bridge()->WriteItem(kKey1, kValue2);
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics9);

  // Put, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics10 = bridge()->WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics10);

  // Data, connect, delete.
  EntitySpecifics specifics11 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics11);
  worker()->VerifyNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Data, delete, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Connect, data, delete.
  EntitySpecifics specifics12 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnPendingCommitDataLoaded();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics12);
  worker()->VerifyNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Connect, delete, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Delete, data, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Delete, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);
}

// Tests cases where pending data loads synchronously.
TEST_F(SharedModelTypeProcessorTest, LoadPendingSynchronous) {
  // Model, sync.
  EntitySpecifics specifics1 = ResetStateWriteItem(kKey1, kValue1);
  bridge()->ExpectSynchronousDataCallback();
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics1);

  // Sync, model.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  OnSyncStarting();
  bridge()->ExpectSynchronousDataCallback();
  InitializeToMetadataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics2);
}

// This test covers race conditions during loading a pending delete. All cases
// start with no processor and one item with a pending delete. There are two
// different events that can occur in any order once metadata is loaded, since
// for a deletion there is no data to load:
//
// - Sync gets connected.
// - Optionally, a put or delete happens to the item (repeated deletes should be
//   handled properly).
//
// This results in 1 + 4 = 5 orderings of the events.
TEST_F(SharedModelTypeProcessorTest, LoadPendingDelete) {
  // Connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Connect, put.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EntitySpecifics specifics1 = bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);
  worker()->VerifyNthPendingCommit(1, kHash1, specifics1);

  // Put, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics2);

  // Connect, delete.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);
  worker()->VerifyNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Delete, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  bridge()->DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, kEmptySpecifics);
}

// Test that loading a committed item does not queue another commit.
TEST_F(SharedModelTypeProcessorTest, LoadCommited) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  ResetState(true);

  // Test that a new processor loads the metadata without committing.
  InitializeToReadyState();
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(SharedModelTypeProcessorTest, LocalCreateItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->WriteItem(kKey1, kValue1);

  // Verify the commit request this operation has triggered.
  worker()->VerifyPendingCommits({kHash1});
  const CommitRequestData& tag1_request_data =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& tag1_data = tag1_request_data.entity.value();

  EXPECT_EQ(kUncommittedVersion, tag1_request_data.base_version);
  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ(kKey1, tag1_data.non_unique_name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ(kKey1, tag1_data.specifics.preference().name());
  EXPECT_EQ(kValue1, tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_FALSE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());

  worker()->AckOnePendingCommit();
  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata acked_metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(acked_metadata.has_server_id());
  EXPECT_EQ(1, acked_metadata.sequence_number());
  EXPECT_EQ(1, acked_metadata.acked_sequence_number());
  EXPECT_EQ(1, acked_metadata.server_version());
}

// Test that commit only types are deleted after commit response.
TEST_F(SharedModelTypeProcessorTest, CommitOnlySimple) {
  ResetState(false, true);
  InitializeToReadyState();

  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  worker()->VerifyPendingCommits({kHash1});
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
}

// Test that commit only types maintain tracking of entities while unsynced
// changes exist.
TEST_F(SharedModelTypeProcessorTest, CommitOnlyUnsyncedChanges) {
  ResetState(false, true);
  InitializeToReadyState();

  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({kHash1});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  bridge()->WriteItem(kKey1, kValue2);
  worker()->VerifyPendingCommits({kHash1, kHash1});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  worker()->AckOnePendingCommit();
  worker()->VerifyPendingCommits({kHash1});
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());

  worker()->AckOnePendingCommit();
  worker()->VerifyPendingCommits(std::vector<std::string>());
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
}

// Test that an error applying metadata changes from a commit response is
// propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, ErrorApplyingAck) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->AckOnePendingCommit();
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
TEST_F(SharedModelTypeProcessorTest, LocalUpdateItemWithOverrides) {
  const std::string kId1 = "cid1";
  const std::string kId2 = "cid2";

  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  std::unique_ptr<EntityData> entity_data = base::MakeUnique<EntityData>();
  entity_data->specifics.mutable_preference()->set_name(kKey1);
  entity_data->specifics.mutable_preference()->set_value(kValue1);

  entity_data->non_unique_name = kKey1;
  entity_data->client_tag_hash = kHash3;
  entity_data->id = kId1;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().metadata_count());
  const EntityData& out_entity1 =
      worker()->GetLatestPendingCommitForHash(kHash1).entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  EXPECT_EQ(kId1, out_entity1.id);
  EXPECT_NE(kHash3, out_entity1.client_tag_hash);
  EXPECT_EQ(kValue1, out_entity1.specifics.preference().value());
  EXPECT_EQ(kId1, metadata_v1.server_id());
  EXPECT_EQ(metadata_v1.client_tag_hash(), out_entity1.client_tag_hash);

  entity_data = base::MakeUnique<EntityData>();
  // This is a sketchy move here, changing the name will change the generated
  // storage key and client tag values.
  entity_data->specifics.mutable_preference()->set_name(kKey2);
  entity_data->specifics.mutable_preference()->set_value(kValue2);
  entity_data->non_unique_name = kKey2;
  entity_data->client_tag_hash = kHash3;
  // Make sure ID isn't overwritten either.
  entity_data->id = kId2;
  bridge()->WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().metadata_count());
  const EntityData& out_entity2 =
      worker()->GetLatestPendingCommitForHash(kHash1).entity.value();
  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);

  EXPECT_EQ(kValue2, out_entity2.specifics.preference().value());
  // Should still see old cid1 value, override is not respected on update.
  EXPECT_EQ(kId1, out_entity2.id);
  EXPECT_EQ(kId1, metadata_v2.server_id());
  EXPECT_EQ(metadata_v2.client_tag_hash(), out_entity2.client_tag_hash);

  // Specifics have changed so the hashes should not match.
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Creates a new local item then modifies it.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(SharedModelTypeProcessorTest, LocalUpdateItem) {
  InitializeToReadyState();

  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({kHash1});

  const CommitRequestData& request_data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& data_v1 = request_data_v1.entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({kHash1, kHash1});

  const CommitRequestData& request_data_v2 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& data_v2 = request_data_v2.entity.value();
  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(request_data_v2.sequence_number, request_data_v1.sequence_number);
  EXPECT_EQ(data_v1.specifics.preference().value(), kValue1);

  // Perform a thorough examination of the update-generated request.
  EXPECT_EQ(kUncommittedVersion, request_data_v2.base_version);
  EXPECT_TRUE(data_v2.id.empty());
  EXPECT_FALSE(data_v2.creation_time.is_null());
  EXPECT_FALSE(data_v2.modification_time.is_null());
  EXPECT_EQ(kKey1, data_v2.non_unique_name);
  EXPECT_FALSE(data_v2.is_deleted());
  EXPECT_EQ(kKey1, data_v2.specifics.preference().name());
  EXPECT_EQ(kValue2, data_v2.specifics.preference().value());

  EXPECT_FALSE(metadata_v1.has_server_id());
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  EXPECT_FALSE(metadata_v2.has_server_id());
  EXPECT_FALSE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  EXPECT_EQ(metadata_v1.client_tag_hash(), metadata_v2.client_tag_hash());
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Tests that a local update that doesn't change specifics doesn't generate a
// commit request.
TEST_F(SharedModelTypeProcessorTest, LocalUpdateItemRedundant) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().metadata_count());
  worker()->VerifyPendingCommits({kHash1});

  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({kHash1});
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(SharedModelTypeProcessorTest, ServerCreateItem) {
  InitializeToReadyState();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  const EntityData& data = db().GetData(kKey1);
  EXPECT_FALSE(data.id.empty());
  EXPECT_EQ(kKey1, data.specifics.preference().name());
  EXPECT_EQ(kValue1, data.specifics.preference().value());
  EXPECT_FALSE(data.creation_time.is_null());
  EXPECT_FALSE(data.modification_time.is_null());
  EXPECT_EQ(kKey1, data.non_unique_name);
  EXPECT_FALSE(data.is_deleted());

  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_TRUE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(0, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(1, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
}

// Test that an error applying changes from a server update is
// propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, ErrorApplyingUpdate) {
  InitializeToReadyState();
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(SharedModelTypeProcessorTest, ServerUpdateItem) {
  InitializeToReadyState();

  // Local add writes data and metadata; ack writes metadata again.
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(2U, db().metadata_change_count());

  // Redundant update from server doesn't write data but updates metadata.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(3U, db().metadata_change_count());

  // A reflection (update already received) is ignored completely.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1),
                             0 /* version_offset */);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(3U, db().metadata_change_count());
}

// Tests locally deleting an acknowledged item.
TEST_F(SharedModelTypeProcessorTest, LocalDeleteItem) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(1, metadata_v1.acked_sequence_number());
  EXPECT_EQ(1, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  // Metadata is not removed until the commit response comes back.
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({kHash1});

  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());
  EXPECT_EQ(1, metadata_v2.server_version());

  // Ack the delete and check that the metadata is cleared.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata_v3 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v3.is_deleted());
  EXPECT_EQ(1, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(3, metadata_v3.server_version());
}

// Tests creating and deleting an item locally before receiving a commit
// response, then getting the commit responses.
TEST_F(SharedModelTypeProcessorTest, LocalDeleteItemInterleaved) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  worker()->VerifyPendingCommits({kHash1});
  const CommitRequestData& data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);

  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->VerifyPendingCommits({kHash1, kHash1});

  const CommitRequestData& data_v2 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  EXPECT_GT(data_v2.sequence_number, data_v1.sequence_number);
  EXPECT_TRUE(data_v2.entity->id.empty());
  EXPECT_EQ(kUncommittedVersion, data_v2.base_version);
  EXPECT_TRUE(data_v2.entity->is_deleted());

  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  // A response for the first commit doesn't change much.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, ProcessorEntityCount());

  const EntityMetadata metadata_v3 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v3.is_deleted());
  EXPECT_EQ(2, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(1, metadata_v3.server_version());

  worker()->AckOnePendingCommit();
  // The delete was acked so the metadata should now be cleared.
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(SharedModelTypeProcessorTest, ServerDeleteItem) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  worker()->TombstoneFromServer(kHash1);
  // Delete from server should clear the data and all the metadata.
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Create item again.
  WriteItemAndAck(kKey1, kValue1);
  const EntityMetadata metadata = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(1, metadata.acked_sequence_number());
  EXPECT_EQ(3, metadata.server_version());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, LocalDeleteUnknown) {
  InitializeToReadyState();
  bridge()->DeleteItem(kKey1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, ServerDeleteUnknown) {
  InitializeToReadyState();
  worker()->TombstoneFromServer(kHash1);
  EXPECT_EQ(0U, db().data_count());
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(SharedModelTypeProcessorTest, TwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_count());
  EXPECT_EQ(1U, db().metadata_count());
  const EntityMetadata metadata1 = db().GetMetadata(kKey1);

  // There should be one commit request for this item only.
  worker()->VerifyPendingCommits({kHash1});

  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_EQ(2U, db().data_count());
  EXPECT_EQ(2U, db().metadata_count());
  const EntityMetadata metadata2 = db().GetMetadata(kKey2);

  // The second write should trigger another single-item commit request.
  worker()->VerifyPendingCommits({kHash1, kHash2});

  EXPECT_FALSE(metadata1.is_deleted());
  EXPECT_EQ(1, metadata1.sequence_number());
  EXPECT_EQ(0, metadata1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata1.server_version());

  EXPECT_FALSE(metadata2.is_deleted());
  EXPECT_EQ(1, metadata2.sequence_number());
  EXPECT_EQ(0, metadata2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata2.server_version());
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionChangesMatch) {
  InitializeToReadyState();
  EntitySpecifics specifics = bridge()->WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(kValue1, db().GetValue(kKey1));
  EXPECT_EQ(1U, db().metadata_change_count());
  EXPECT_EQ(kUncommittedVersion, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({kHash1});
  worker()->VerifyNthPendingCommit(0, kHash1, specifics);

  // Changes match doesn't call ResolveConflict.
  worker()->UpdateFromServer(kHash1, specifics);

  // Updated metadata but not data; no new commit request.
  EXPECT_EQ(1U, db().data_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({kHash1});
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseLocal) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::UseLocal());

  // Change value locally and at the same time simulate conflicting update from
  // server.
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue3));

  // Updated metadata but not data; new commit request.
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(4U, db().metadata_change_count());
  EXPECT_EQ(2, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({kHash1, kHash1});
  worker()->VerifyNthPendingCommit(1, kHash1, specifics2);
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseRemote) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->SetConflictResolution(ConflictResolution::UseRemote());
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().metadata_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({kHash1});
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseNew) {
  InitializeToReadyState();
  bridge()->WriteItem(kKey1, kValue1);
  bridge()->SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(2U, db().data_change_count());
  EXPECT_EQ(kValue3, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().metadata_change_count());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->VerifyPendingCommits({kHash1, kHash1});
  worker()->VerifyNthPendingCommit(1, kHash1,
                                   GenerateSpecifics(kKey1, kValue3));
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(SharedModelTypeProcessorTest, Disconnect) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  DisconnectSync();

  // The third item is added after stopping.
  bridge()->WriteItem(kKey3, kValue3);

  // Reconnect.
  OnSyncStarting();

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EXPECT_EQ(2U, worker()->GetNthPendingCommit(0).size());

  // The first item was already in sync.
  EXPECT_FALSE(worker()->HasPendingCommitForHash(kHash1));

  // The second item's commit was interrupted and should be retried.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  // The third item's commit was not started until the reconnect.
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash3));
}

// Test proper handling of disable and re-enable.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on re-enable.
TEST_F(SharedModelTypeProcessorTest, Disable) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck(kKey1, kValue1);

  // The second item has a commit request in progress.
  bridge()->WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  bridge()->DisableSync();
  EXPECT_FALSE(type_processor()->IsTrackingMetadata());

  // The third item is added after disable.
  bridge()->WriteItem(kKey3, kValue3);

  // Now we re-enable.
  OnSyncStarting();
  worker()->UpdateFromServer();
  EXPECT_TRUE(type_processor()->IsTrackingMetadata());

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  worker()->VerifyPendingCommits({kHash1, kHash2, kHash3});
}

// Test re-encrypt everything when desired encryption key changes.
TEST_F(SharedModelTypeProcessorTest, ReEncryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);
  // Create another item and don't wait for its commit response.
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey2, kValue2);
  worker()->VerifyPendingCommits({kHash2});
  EXPECT_EQ(1U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(1U, db().GetMetadata(kKey2).sequence_number());

  // Receive notice that the account's desired encryption key has changed.
  worker()->UpdateWithEncryptionKey("k1");
  // Tag 2 is recommitted immediately because the data was in memory.
  ASSERT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash2, specifics2);
  // Sequence numbers in the store are updated.
  EXPECT_EQ(2U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(2U, db().GetMetadata(kKey2).sequence_number());

  // Tag 1 needs to go to the store to load its data before recommitting.
  OnPendingCommitDataLoaded();
  ASSERT_EQ(3U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(2, kHash1, specifics1);
}

// Test that an error loading pending commit data for re-encryption is
// propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, ReEncryptErrorLoadingData) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  bridge()->ErrorOnNextCall();
  ExpectError();
  worker()->UpdateWithEncryptionKey("k1");
}

// Test receipt of updates with new and old keys.
TEST_F(SharedModelTypeProcessorTest, ReEncryptUpdatesWithNewKey) {
  InitializeToReadyState();

  // Receive an unencrypted update.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  ASSERT_EQ(0U, worker()->GetNumPendingCommits());

  UpdateResponseDataList update;
  // Receive an entity with old encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      kHash2, GenerateSpecifics(kKey2, kValue2), 1, "k1"));
  // Receive an entity with up-to-date encryption as part of the update.
  update.push_back(worker()->GenerateUpdateData(
      kHash3, GenerateSpecifics(kKey3, kValue3), 1, "k2"));
  // Set desired encryption key to k2 to force updates to some items.
  worker()->UpdateWithEncryptionKey("k2", update);

  // kKey2 needed to be re-encrypted and had data so it was queued immediately.
  worker()->VerifyPendingCommits({kHash2});
  OnPendingCommitDataLoaded();
  // kKey1 needed data so once that's loaded, it is also queued.
  worker()->VerifyPendingCommits({kHash2, kHash1});

  // Receive a separate update that was encrypted with key k1.
  worker()->UpdateFromServer(kHash4, GenerateSpecifics(kKey4, kValue1), 1,
                             "k1");
  // Receipt of updates encrypted with old key also forces a re-encrypt commit.
  worker()->VerifyPendingCommits({kHash2, kHash1, kHash4});

  // Receive an update that was encrypted with key k2.
  worker()->UpdateFromServer(kHash5, GenerateSpecifics(kKey5, kValue1), 1,
                             "k2");
  // That was the correct key, so no re-encryption is required.
  worker()->VerifyPendingCommits({kHash2, kHash1, kHash4});
}

// Test that re-encrypting enqueues the right data for USE_LOCAL conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseLocal) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");
  EntitySpecifics specifics = bridge()->WriteItem(kKey1, kValue2);
  worker()->VerifyPendingCommits({kHash1});

  bridge()->SetConflictResolution(ConflictResolution::UseLocal());
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue3), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash1, specifics);
  EXPECT_EQ(kValue2, db().GetValue(kKey1));

  // GetData was launched as a result of UpdateWithEncryptionKey(). Since the
  // conflict resolution encrypted all entities, the GetData result should be
  // ignored.
  OnPendingCommitDataLoaded();
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
}

// Test that re-encrypting enqueues the right data for USE_REMOTE conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseRemote) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  bridge()->WriteItem(kKey1, kValue1);

  bridge()->SetConflictResolution(ConflictResolution::UseRemote());
  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GenerateSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash1, specifics);
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
}

// Test that re-encrypting enqueues the right data for USE_NEW conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseNew) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  bridge()->WriteItem(kKey1, kValue1);

  bridge()->SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash1,
                                   GenerateSpecifics(kKey1, kValue3));
  EXPECT_EQ(kValue3, db().GetValue(kKey1));
}

TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictWhileLoading) {
  InitializeToReadyState();
  // Create item and ack so its data is no longer cached.
  WriteItemAndAck(kKey1, kValue1);
  // Update key so that it needs to fetch data to re-commit.
  worker()->UpdateWithEncryptionKey("k1");
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GenerateSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics);
  EXPECT_EQ(kValue2, db().GetValue(kKey1));

  // Data load completing shouldn't change anything.
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
}

// Tests that a real remote change wins over a local encryption-only change.
TEST_F(SharedModelTypeProcessorTest, IgnoreLocalEncryption) {
  InitializeToReadyState();
  EntitySpecifics specifics = WriteItemAndAck(kKey1, kValue1);
  worker()->UpdateWithEncryptionKey("k1");
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics);

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
}

// Tests that a real local change wins over a remote encryption-only change.
TEST_F(SharedModelTypeProcessorTest, IgnoreRemoteEncryption) {
  InitializeToReadyState();
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);

  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue2);
  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash1, specifics2);
}

// Same as above but with two commit requests before one ack.
TEST_F(SharedModelTypeProcessorTest, IgnoreRemoteEncryptionInterleaved) {
  InitializeToReadyState();
  // WriteAndAck entity to get id from the server.
  WriteItemAndAck(kKey1, kValue1);
  EntitySpecifics specifics1 = bridge()->WriteItem(kKey1, kValue2);
  EntitySpecifics specifics2 = bridge()->WriteItem(kKey1, kValue3);
  worker()->AckOnePendingCommit();
  // kValue2 is now the base value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(0, kHash1, specifics2);

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->VerifyNthPendingCommit(1, kHash1, specifics2);
}

// Tests that UpdateStorageKey propagates storage key to ProcessorEntityTracker
// and updates corresponding entity's metadata in MetadataChangeList, and
// UntrackEntity will remove corresponding ProcessorEntityTracker and do not add
// any entity's metadata into MetadataChangeList.
TEST_F(SharedModelTypeProcessorTest, UpdateStorageKey) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeModelTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeSyncData.
  UpdateResponseDataList updates;
  updates.push_back(
      worker()->GenerateUpdateData(kHash1, GenerateSpecifics(kKey1, kValue1)));
  // Create update which will be ignored by bridge.
  updates.push_back(
      worker()->GenerateUpdateData(kHash3, GenerateSpecifics(kKey3, kValue3)));
  bridge()->SetKeyToIgnore(kKey3);
  worker()->UpdateFromServer(updates);
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(1U, ProcessorEntityCount());
  // Metadata should be written under kKey1. This means that UpdateStorageKey
  // was called and value of storage key got propagated to MetadataChangeList.
  EXPECT_TRUE(db().HasMetadata(kKey1));
  EXPECT_EQ(1U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());

  // Local update of kKey1 should affect the same entity. This ensures that
  // storage key to client tag hash mapping was updated on the previous step.
  bridge()->WriteItem(kKey1, kValue2);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().metadata_count());

  // Second update from server should be handled by ApplySyncChanges. Similarly
  // It should call UpdateStorageKey, not GetStorageKey.
  worker()->UpdateFromServer(kHash2, GenerateSpecifics(kKey2, kValue2));
  EXPECT_EQ(1, bridge()->apply_call_count());
  EXPECT_TRUE(db().HasMetadata(kKey2));
  EXPECT_EQ(2U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

// Tests that reencryption scenario works correctly for types that don't support
// GetStorageKey(). When update from server delivers updated encryption key, all
// entities should be reencrypted including new entity that just got received
// from server.
TEST_F(SharedModelTypeProcessorTest, ReencryptionWithEmptyStorageKeys) {
  bridge()->SetSupportsGetStorageKey(false);
  InitializeToReadyState();

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(
      kHash1, GenerateSpecifics(kKey1, kValue1), 1, "ek1"));
  worker()->UpdateWithEncryptionKey("ek2", update);
  worker()->VerifyPendingCommits({kHash1});
}

// Tests that UntrackEntity won't propagate storage key to
// ProcessorEntityTracker, and no entity's metadata are added into
// MetadataChangeList.
TEST_F(SharedModelTypeProcessorTest, UntrackEntity) {
  // Setup bridge to not support calls to GetStorageKey. This will cause
  // FakeModelTypeSyncBridge to call UpdateStorageKey for new entities and will
  // DCHECK if GetStorageKey gets called.
  bridge()->SetSupportsGetStorageKey(false);
  bridge()->SetKeyToIgnore(kKey1);
  ModelReadyToSync();
  OnSyncStarting();

  // Initial update from server should be handled by MergeSyncData.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1, bridge()->merge_call_count());
  EXPECT_EQ(0U, ProcessorEntityCount());
  // Metadata should not be written under kUntrackKey1. This means that
  // UntrackEntity was called and corresponding ProcessorEntityTracker is
  // removed and no storage key got propagated to MetadataChangeList.
  EXPECT_FALSE(db().HasMetadata(kHash1));
  EXPECT_EQ(0U, db().metadata_count());
  EXPECT_EQ(0, bridge()->get_storage_key_call_count());
}

}  // namespace syncer
