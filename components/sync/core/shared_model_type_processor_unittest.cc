// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/shared_model_type_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync/api/fake_model_type_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/core/activation_context.h"
#include "components/sync/core/data_batch_impl.h"
#include "components/sync/core/non_blocking_sync_common.h"
#include "components/sync/core/simple_metadata_change_list.h"
#include "components/sync/core/test/data_type_error_handler_mock.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/test/engine/mock_model_type_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntitySpecifics;
using sync_pb::EntityMetadata;
using sync_pb::DataTypeState;

namespace syncer_v2 {

namespace {

std::string ClientTagFromKey(const std::string& key) {
  return "ClientTag_" + key;
}

std::string TagHashFromKey(const std::string& key) {
  return syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES,
                                                ClientTagFromKey(key));
}

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kKey4[] = "key4";
const char kKey5[] = "key5";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";
const std::string kHash1(TagHashFromKey(kKey1));
const std::string kHash2(TagHashFromKey(kKey2));
const std::string kHash3(TagHashFromKey(kKey3));
const std::string kHash4(TagHashFromKey(kKey4));
const std::string kHash5(TagHashFromKey(kKey5));

// Typically used for verification after a delete. The specifics given to the
// worker/processor will not have been initialized and thus empty.
const EntitySpecifics kEmptySpecifics;

EntitySpecifics GenerateSpecifics(const std::string& name,
                                  const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(name);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

std::unique_ptr<EntityData> GenerateEntityData(const std::string& key,
                                               const std::string& value) {
  std::unique_ptr<EntityData> entity_data = base::MakeUnique<EntityData>();
  entity_data->client_tag_hash = TagHashFromKey(key);
  entity_data->specifics = GenerateSpecifics(key, value);
  entity_data->non_unique_name = key;
  return entity_data;
}

// It is intentionally very difficult to copy an EntityData, as in normal code
// we never want to. However, since we store the data as an EntityData for the
// test code here, this function is needed to manually copy it.
std::unique_ptr<EntityData> CopyEntityData(const EntityData& old_data) {
  std::unique_ptr<EntityData> new_data(new EntityData());
  new_data->id = old_data.id;
  new_data->client_tag_hash = old_data.client_tag_hash;
  new_data->non_unique_name = old_data.non_unique_name;
  new_data->specifics = old_data.specifics;
  new_data->creation_time = old_data.creation_time;
  new_data->modification_time = old_data.modification_time;
  return new_data;
}

syncer::SyncError CreateSyncError(syncer::SyncError::ErrorType error_type) {
  return syncer::SyncError(FROM_HERE, error_type, "TestError",
                           syncer::PREFERENCES);
}

// A basic in-memory storage mechanism for data and metadata. This makes it
// easier to test more complex behaviors involving when entities are written,
// committed, etc. Having a separate class helps keep the main one cleaner.
class SimpleStore {
 public:
  void PutData(const std::string& key, const EntityData& data) {
    data_change_count_++;
    data_store_[key] = CopyEntityData(data);
  }

  void PutMetadata(const std::string& key, const EntityMetadata& metadata) {
    metadata_change_count_++;
    metadata_store_[key] = metadata;
  }

  void RemoveData(const std::string& key) {
    data_change_count_++;
    data_store_.erase(key);
  }

  void RemoveMetadata(const std::string& key) {
    metadata_change_count_++;
    metadata_store_.erase(key);
  }

  bool HasData(const std::string& key) const {
    return data_store_.find(key) != data_store_.end();
  }

  bool HasMetadata(const std::string& key) const {
    return metadata_store_.find(key) != metadata_store_.end();
  }

  const std::map<std::string, std::unique_ptr<EntityData>>& GetAllData() const {
    return data_store_;
  }

  const EntityData& GetData(const std::string& key) const {
    return *data_store_.find(key)->second;
  }

  const std::string& GetValue(const std::string& key) const {
    return GetData(key).specifics.preference().value();
  }

  const EntityMetadata& GetMetadata(const std::string& key) const {
    return metadata_store_.find(key)->second;
  }

  size_t DataCount() const { return data_store_.size(); }
  size_t MetadataCount() const { return metadata_store_.size(); }

  size_t DataChangeCount() const { return data_change_count_; }
  size_t MetadataChangeCount() const { return metadata_change_count_; }

  const DataTypeState& data_type_state() const { return data_type_state_; }

  void set_data_type_state(const DataTypeState& data_type_state) {
    data_type_state_ = data_type_state;
  }

  std::unique_ptr<MetadataBatch> CreateMetadataBatch() const {
    std::unique_ptr<MetadataBatch> metadata_batch(new MetadataBatch());
    metadata_batch->SetDataTypeState(data_type_state_);
    for (auto it = metadata_store_.begin(); it != metadata_store_.end(); it++) {
      metadata_batch->AddMetadata(it->first, it->second);
    }
    return metadata_batch;
  }

  void Reset() {
    data_change_count_ = 0;
    metadata_change_count_ = 0;
    data_store_.clear();
    metadata_store_.clear();
    data_type_state_.Clear();
  }

 private:
  size_t data_change_count_ = 0;
  size_t metadata_change_count_ = 0;
  std::map<std::string, std::unique_ptr<EntityData>> data_store_;
  std::map<std::string, EntityMetadata> metadata_store_;
  DataTypeState data_type_state_;
};

}  // namespace

// Tests the various functionality of SharedModelTypeProcessor.
//
// The processor sits between the service (implemented by this test class) and
// the worker, which is represented by a MockModelTypeWorker. This test suite
// exercises the initialization flows (whether initial sync is done, performing
// the initial merge, etc) as well as normal functionality:
//
// - Initialization before the initial sync and merge correctly performs a merge
//   and initializes the metadata in storage.
// - Initialization after the initial sync correctly loads metadata and queues
//   any pending commits.
// - Put and Delete calls from the service result in the correct metadata in
//   storage and the correct commit requests on the worker side.
// - Updates and commit responses from the worker correctly affect data and
//   metadata in storage on the service side.
class SharedModelTypeProcessorTest : public ::testing::Test,
                                     public FakeModelTypeService {
 public:
  SharedModelTypeProcessorTest()
      : FakeModelTypeService(
            base::Bind(&SharedModelTypeProcessor::CreateAsChangeProcessor)) {}

  ~SharedModelTypeProcessorTest() override { CheckPostConditions(); }

  void InitializeToMetadataLoaded() {
    CreateChangeProcessor();
    DataTypeState data_type_state(db_.data_type_state());
    data_type_state.set_initial_sync_done(true);
    db_.set_data_type_state(data_type_state);
    OnMetadataLoaded();
  }

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState() {
    InitializeToMetadataLoaded();
    if (!data_callback_.is_null()) {
      OnPendingCommitDataLoaded();
    }
    OnSyncStarting();
  }

  void OnMetadataLoaded() {
    type_processor()->OnMetadataLoaded(syncer::SyncError(),
                                       db_.CreateMetadataBatch());
  }

  void OnPendingCommitDataLoaded() {
    DCHECK(!data_callback_.is_null());
    data_callback_.Run();
    data_callback_.Reset();
  }

  void OnSyncStarting() {
    type_processor()->OnSyncStarting(
        &error_handler_,
        base::Bind(&SharedModelTypeProcessorTest::OnReadyToConnect,
                   base::Unretained(this)));
  }

  void DisconnectSync() {
    type_processor()->DisconnectSync();
    worker_ = nullptr;
  }

  // Local data modification.  Emulates signals from the model thread.
  EntitySpecifics WriteItem(const std::string& key, const std::string& value) {
    std::unique_ptr<EntityData> entity_data = GenerateEntityData(key, value);
    EntitySpecifics specifics_copy = entity_data->specifics;
    WriteItem(key, std::move(entity_data));
    return specifics_copy;
  }

  // Overloaded form to allow passing of custom entity data.
  void WriteItem(const std::string& key,
                 std::unique_ptr<EntityData> entity_data) {
    db_.PutData(key, *entity_data);
    if (type_processor()) {
      std::unique_ptr<MetadataChangeList> change_list(
          new SimpleMetadataChangeList());
      type_processor()->Put(key, std::move(entity_data), change_list.get());
      ApplyMetadataChangeList(std::move(change_list));
    }
  }

  // Writes data for |key| and simulates a commit response for it.
  EntitySpecifics WriteItemAndAck(const std::string& key,
                                  const std::string& value) {
    EntitySpecifics specifics = WriteItem(key, value);
    worker()->ExpectPendingCommits({TagHashFromKey(key)});
    worker()->AckOnePendingCommit();
    EXPECT_EQ(0U, worker()->GetNumPendingCommits());
    return specifics;
  }

  void DeleteItem(const std::string& key) {
    db_.RemoveData(key);
    if (type_processor()) {
      std::unique_ptr<MetadataChangeList> change_list(
          new SimpleMetadataChangeList());
      type_processor()->Delete(key, change_list.get());
      ApplyMetadataChangeList(std::move(change_list));
    }
  }

  void ResetState() {
    clear_change_processor();
    db_.Reset();
    worker_ = nullptr;
    CheckPostConditions();
  }

  // Wipes existing DB and simulates a pending update of a server-known item.
  EntitySpecifics ResetStateWriteItem(const std::string& name,
                                      const std::string& value) {
    ResetState();
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, "acked-value");
    EntitySpecifics specifics = WriteItem(name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    clear_change_processor();
    worker_ = nullptr;
    return specifics;
  }

  // Wipes existing DB and simulates a pending deletion of a server-known item.
  void ResetStateDeleteItem(const std::string& name, const std::string& value) {
    ResetState();
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(name, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    DeleteItem(name);
    EXPECT_EQ(1U, ProcessorEntityCount());
    clear_change_processor();
    worker_ = nullptr;
  }

  // Simulates an initial GetUpdates response from the worker with |updates|.
  void OnInitialSyncDone(UpdateResponseDataList updates) {
    DataTypeState data_type_state(db_.data_type_state());
    data_type_state.set_initial_sync_done(true);
    type_processor()->OnUpdateReceived(data_type_state, updates);
  }

  // Overloaded form with no updates.
  void OnInitialSyncDone() { OnInitialSyncDone(UpdateResponseDataList()); }

  // Overloaded form that constructs an update for a single entity.
  void OnInitialSyncDone(const std::string& key, const std::string& value) {
    UpdateResponseDataList updates;
    UpdateResponseData update;
    update.entity = GenerateEntityData(key, value)->PassToPtr();
    updates.push_back(update);
    OnInitialSyncDone(updates);
  }

  // Return the number of entities the processor has metadata for.
  size_t ProcessorEntityCount() const {
    DCHECK(type_processor());
    return type_processor()->entities_.size();
  }

  // Store a resolution for the next call to ResolveConflict. Note that if this
  // is a USE_NEW resolution, the data will only exist for one resolve call.
  void SetConflictResolution(ConflictResolution resolution) {
    conflict_resolution_.reset(new ConflictResolution(std::move(resolution)));
  }

  // Sets the error that the next fallible call to the service will generate.
  void SetServiceError(syncer::SyncError::ErrorType error_type) {
    DCHECK(!service_error_.IsSet());
    service_error_ = CreateSyncError(error_type);
  }

  // Sets the error type that OnReadyToConnect (our StartCallback) expects to
  // receive.
  void ExpectStartError(syncer::SyncError::ErrorType error_type) {
    DCHECK(expected_start_error_ == syncer::SyncError::UNSET);
    expected_start_error_ = error_type;
  }

  const SimpleStore& db() const { return db_; }

  MockModelTypeWorker* worker() { return worker_; }

  SharedModelTypeProcessor* type_processor() const {
    return static_cast<SharedModelTypeProcessor*>(change_processor());
  }

  syncer::DataTypeErrorHandlerMock* error_handler() { return &error_handler_; }

 private:
  void CheckPostConditions() {
    DCHECK(data_callback_.is_null());
    DCHECK(!service_error_.IsSet());
    DCHECK_EQ(syncer::SyncError::UNSET, expected_start_error_);
  }

  void OnReadyToConnect(syncer::SyncError error,
                        std::unique_ptr<ActivationContext> context) {
    if (expected_start_error_ != syncer::SyncError::UNSET) {
      EXPECT_TRUE(error.IsSet());
      EXPECT_EQ(expected_start_error_, error.error_type());
      EXPECT_EQ(nullptr, context);
      expected_start_error_ = syncer::SyncError::UNSET;
      return;
    }

    std::unique_ptr<MockModelTypeWorker> worker(
        new MockModelTypeWorker(context->data_type_state, type_processor()));
    // Keep an unsafe pointer to the commit queue the processor will use.
    worker_ = worker.get();
    // The context contains a proxy to the processor, but this call is
    // side-stepping that completely and connecting directly to the real
    // processor, since these tests are single-threaded and don't need proxies.
    type_processor()->ConnectSync(std::move(worker));
  }

  // FakeModelTypeService overrides.

  std::string GetClientTag(const EntityData& entity_data) override {
    return ClientTagFromKey(entity_data.specifics.preference().name());
  }

  std::string GetStorageKey(const EntityData& entity_data) override {
    return entity_data.specifics.preference().name();
  }

  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override {
    return std::unique_ptr<MetadataChangeList>(new SimpleMetadataChangeList());
  }

  syncer::SyncError MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_changes,
      EntityDataMap data_map) override {
    if (service_error_.IsSet()) {
      syncer::SyncError error = service_error_;
      service_error_ = syncer::SyncError();
      return error;
    }
    // Commit any local entities that aren't being overwritten by the server.
    const auto& local_data = db_.GetAllData();
    for (auto it = local_data.begin(); it != local_data.end(); it++) {
      if (data_map.find(it->first) == data_map.end()) {
        type_processor()->Put(it->first, CopyEntityData(*it->second),
                              metadata_changes.get());
      }
    }
    // Store any new remote entities.
    for (auto it = data_map.begin(); it != data_map.end(); it++) {
      db_.PutData(it->first, it->second.value());
    }
    ApplyMetadataChangeList(std::move(metadata_changes));
    return syncer::SyncError();
  }

  syncer::SyncError ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_changes,
      EntityChangeList entity_changes) override {
    if (service_error_.IsSet()) {
      syncer::SyncError error = service_error_;
      service_error_ = syncer::SyncError();
      return error;
    }
    for (const EntityChange& change : entity_changes) {
      switch (change.type()) {
        case EntityChange::ACTION_ADD:
          EXPECT_FALSE(db_.HasData(change.storage_key()));
          db_.PutData(change.storage_key(), change.data());
          break;
        case EntityChange::ACTION_UPDATE:
          EXPECT_TRUE(db_.HasData(change.storage_key()));
          db_.PutData(change.storage_key(), change.data());
          break;
        case EntityChange::ACTION_DELETE:
          EXPECT_TRUE(db_.HasData(change.storage_key()));
          db_.RemoveData(change.storage_key());
          break;
      }
    }
    ApplyMetadataChangeList(std::move(metadata_changes));
    return syncer::SyncError();
  }

  void ApplyMetadataChangeList(
      std::unique_ptr<MetadataChangeList> change_list) {
    DCHECK(change_list);
    SimpleMetadataChangeList* changes =
        static_cast<SimpleMetadataChangeList*>(change_list.get());
    const auto& metadata_changes = changes->GetMetadataChanges();
    for (auto it = metadata_changes.begin(); it != metadata_changes.end();
         it++) {
      switch (it->second.type) {
        case SimpleMetadataChangeList::UPDATE:
          db_.PutMetadata(it->first, it->second.metadata);
          break;
        case SimpleMetadataChangeList::CLEAR:
          EXPECT_TRUE(db_.HasMetadata(it->first));
          db_.RemoveMetadata(it->first);
          break;
      }
    }
    if (changes->HasDataTypeStateChange()) {
      const SimpleMetadataChangeList::DataTypeStateChange& state_change =
          changes->GetDataTypeStateChange();
      switch (state_change.type) {
        case SimpleMetadataChangeList::UPDATE:
          db_.set_data_type_state(state_change.state);
          break;
        case SimpleMetadataChangeList::CLEAR:
          db_.set_data_type_state(DataTypeState());
          break;
      }
    }
  }

  void GetData(StorageKeyList keys, DataCallback callback) override {
    if (service_error_.IsSet()) {
      data_callback_ = base::Bind(callback, service_error_, nullptr);
      service_error_ = syncer::SyncError();
      return;
    }
    std::unique_ptr<DataBatchImpl> batch(new DataBatchImpl());
    for (const std::string& key : keys) {
      DCHECK(db_.HasData(key)) << "No data for " << key;
      batch->Put(key, CopyEntityData(db_.GetData(key)));
    }
    data_callback_ =
        base::Bind(callback, syncer::SyncError(), base::Passed(&batch));
  }

  ConflictResolution ResolveConflict(
      const EntityData& local_data,
      const EntityData& remote_data) const override {
    DCHECK(conflict_resolution_);
    return std::move(*conflict_resolution_);
  }

  std::unique_ptr<ConflictResolution> conflict_resolution_;

  // This sets ThreadTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::MessageLoop sync_loop_;

  // The current mock queue, which is owned by |type_processor()|.
  MockModelTypeWorker* worker_;

  // Stores the data callback between GetData() and OnPendingCommitDataLoaded().
  base::Closure data_callback_;

  // Contains all of the data and metadata state for these tests.
  SimpleStore db_;

  // The processor's error handler.
  syncer::DataTypeErrorHandlerMock error_handler_;

  // The error to produce on the next service call.
  syncer::SyncError service_error_;

  // The error to expect in OnReadyToConnect().
  syncer::SyncError::ErrorType expected_start_error_ = syncer::SyncError::UNSET;
};

// Test that an initial sync handles local and remote items properly.
TEST_F(SharedModelTypeProcessorTest, InitialSync) {
  CreateChangeProcessor();
  OnMetadataLoaded();
  OnSyncStarting();

  // Local write before initial sync.
  WriteItem(kKey1, kValue1);

  // Has data, but no metadata, entity in the processor, or commit request.
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  // Initial sync with one server item.
  OnInitialSyncDone(kKey2, kValue2);

  // Now have data and metadata for both items, as well as a commit request for
  // the local item.
  EXPECT_EQ(2U, db().DataCount());
  EXPECT_EQ(2U, db().MetadataCount());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(0, db().GetMetadata(kKey2).sequence_number());
  worker()->ExpectPendingCommits({kHash1});
}

// Test that an error during the merge is propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, InitialSyncError) {
  CreateChangeProcessor();
  OnMetadataLoaded();
  OnSyncStarting();

  SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  error_handler()->ExpectError(syncer::SyncError::DATATYPE_ERROR);
  OnInitialSyncDone();
}

// Test that errors before it's called are passed to |start_callback| correctly.
TEST_F(SharedModelTypeProcessorTest, StartErrors) {
  CreateChangeProcessor();
  type_processor()->OnMetadataLoaded(
      CreateSyncError(syncer::SyncError::DATATYPE_ERROR), nullptr);
  ExpectStartError(syncer::SyncError::DATATYPE_ERROR);
  OnSyncStarting();

  // Test OnSyncStarting happening first.
  ResetState();
  CreateChangeProcessor();
  OnSyncStarting();
  ExpectStartError(syncer::SyncError::DATATYPE_ERROR);
  type_processor()->OnMetadataLoaded(
      CreateSyncError(syncer::SyncError::DATATYPE_ERROR), nullptr);

  // Test an error loading pending data.
  ResetStateWriteItem(kKey1, kValue1);
  SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  ExpectStartError(syncer::SyncError::DATATYPE_ERROR);
  OnSyncStarting();
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
  worker()->ExpectNthPendingCommit(0, kHash1, specifics1);

  // Connect, data.
  EntitySpecifics specifics2 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics2);

  // Data, connect, put.
  EntitySpecifics specifics3 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EntitySpecifics specifics4 = WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics3);
  worker()->ExpectNthPendingCommit(1, kHash1, specifics4);

  // Data, put, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  EntitySpecifics specifics5 = WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics5);

  // Connect, data, put.
  EntitySpecifics specifics6 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnPendingCommitDataLoaded();
  EntitySpecifics specifics7 = WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics6);
  worker()->ExpectNthPendingCommit(1, kHash1, specifics7);

  // Connect, put, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EntitySpecifics specifics8 = WriteItem(kKey1, kValue2);
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics8);

  // Put, data, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics9 = WriteItem(kKey1, kValue2);
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics9);

  // Put, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics10 = WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics10);

  // Data, connect, delete.
  EntitySpecifics specifics11 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics11);
  worker()->ExpectNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Data, delete, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnPendingCommitDataLoaded();
  DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Connect, data, delete.
  EntitySpecifics specifics12 = ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnPendingCommitDataLoaded();
  DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics12);
  worker()->ExpectNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Connect, delete, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  DeleteItem(kKey1);
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Delete, data, connect.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  DeleteItem(kKey1);
  OnPendingCommitDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Delete, connect, data.
  ResetStateWriteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(nullptr, worker());
  OnPendingCommitDataLoaded();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);
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
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);

  // Connect, put.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  EntitySpecifics specifics1 = WriteItem(kKey1, kValue2);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);
  worker()->ExpectNthPendingCommit(1, kHash1, specifics1);

  // Put, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  EntitySpecifics specifics2 = WriteItem(kKey1, kValue2);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics2);

  // Connect, delete.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  DeleteItem(kKey1);
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);
  worker()->ExpectNthPendingCommit(1, kHash1, kEmptySpecifics);

  // Delete, connect.
  ResetStateDeleteItem(kKey1, kValue1);
  InitializeToMetadataLoaded();
  DeleteItem(kKey1);
  OnSyncStarting();
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, kEmptySpecifics);
}

// Test that loading a committed item does not queue another commit.
TEST_F(SharedModelTypeProcessorTest, LoadCommited) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  clear_change_processor();

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

  WriteItem(kKey1, kValue1);

  // Verify the commit request this operation has triggered.
  worker()->ExpectPendingCommits({kHash1});
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

  EXPECT_EQ(1U, db().MetadataCount());
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
  EXPECT_EQ(1U, db().MetadataCount());
  const EntityMetadata acked_metadata = db().GetMetadata(kKey1);
  EXPECT_TRUE(acked_metadata.has_server_id());
  EXPECT_EQ(1, acked_metadata.sequence_number());
  EXPECT_EQ(1, acked_metadata.acked_sequence_number());
  EXPECT_EQ(1, acked_metadata.server_version());
}

// Test that an error applying metadata changes from a commit response is
// propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, ErrorApplyingAck) {
  InitializeToReadyState();
  WriteItem(kKey1, kValue1);
  SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  error_handler()->ExpectError(syncer::SyncError::DATATYPE_ERROR);
  worker()->AckOnePendingCommit();
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
TEST_F(SharedModelTypeProcessorTest, LocalUpdateItemWithOverrides) {
  const std::string kId1 = "cid1";
  const std::string kId2 = "cid2";

  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  std::unique_ptr<EntityData> entity_data = base::WrapUnique(new EntityData());
  entity_data->specifics.mutable_preference()->set_name(kKey1);
  entity_data->specifics.mutable_preference()->set_value(kValue1);

  entity_data->non_unique_name = kKey1;
  entity_data->client_tag_hash = kHash3;
  entity_data->id = kId1;
  WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().MetadataCount());
  const EntityData& out_entity1 =
      worker()->GetLatestPendingCommitForHash(kHash1).entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  EXPECT_EQ(kId1, out_entity1.id);
  EXPECT_NE(kHash3, out_entity1.client_tag_hash);
  EXPECT_EQ(kValue1, out_entity1.specifics.preference().value());
  EXPECT_EQ(kId1, metadata_v1.server_id());
  EXPECT_EQ(metadata_v1.client_tag_hash(), out_entity1.client_tag_hash);

  entity_data.reset(new EntityData());
  // This is a sketchy move here, changing the name will change the generated
  // storage key and client tag values.
  entity_data->specifics.mutable_preference()->set_name(kKey2);
  entity_data->specifics.mutable_preference()->set_value(kValue2);
  entity_data->non_unique_name = kKey2;
  entity_data->client_tag_hash = kHash3;
  // Make sure ID isn't overwritten either.
  entity_data->id = kId2;
  WriteItem(kKey1, std::move(entity_data));

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  ASSERT_FALSE(worker()->HasPendingCommitForHash(kHash3));
  ASSERT_TRUE(worker()->HasPendingCommitForHash(kHash1));
  EXPECT_EQ(1U, db().MetadataCount());
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

  WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().MetadataCount());
  worker()->ExpectPendingCommits({kHash1});

  const CommitRequestData& request_data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);
  const EntityData& data_v1 = request_data_v1.entity.value();
  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);

  WriteItem(kKey1, kValue2);
  EXPECT_EQ(1U, db().MetadataCount());
  worker()->ExpectPendingCommits({kHash1, kHash1});

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
  WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().MetadataCount());
  worker()->ExpectPendingCommits({kHash1});

  WriteItem(kKey1, kValue1);
  worker()->ExpectPendingCommits({kHash1});
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(SharedModelTypeProcessorTest, ServerCreateItem) {
  InitializeToReadyState();
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
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
  SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  error_handler()->ExpectError(syncer::SyncError::DATATYPE_ERROR);
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
}

// Thoroughly tests the data generated by a server item creation.
TEST_F(SharedModelTypeProcessorTest, ServerUpdateItem) {
  InitializeToReadyState();

  // Local add writes data and metadata; ack writes metadata again.
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(2U, db().MetadataChangeCount());

  // Redundant update from server doesn't write data but updates metadata.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1));
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(3U, db().MetadataChangeCount());

  // A reflection (update already received) is ignored completely.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue1),
                             0 /* version_offset */);
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(3U, db().MetadataChangeCount());
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

  DeleteItem(kKey1);
  EXPECT_EQ(0U, db().DataCount());
  // Metadata is not removed until the commit response comes back.
  EXPECT_EQ(1U, db().MetadataCount());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->ExpectPendingCommits({kHash1});

  const EntityMetadata metadata_v2 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());
  EXPECT_EQ(1, metadata_v2.server_version());

  // Ack the delete and check that the metadata is cleared.
  worker()->AckOnePendingCommit();
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

// Tests creating and deleting an item locally before receiving a commit
// response, then getting the commit responses.
TEST_F(SharedModelTypeProcessorTest, LocalDeleteItemInterleaved) {
  InitializeToReadyState();
  WriteItem(kKey1, kValue1);
  worker()->ExpectPendingCommits({kHash1});
  const CommitRequestData& data_v1 =
      worker()->GetLatestPendingCommitForHash(kHash1);

  const EntityMetadata metadata_v1 = db().GetMetadata(kKey1);
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  DeleteItem(kKey1);
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  EXPECT_EQ(1U, ProcessorEntityCount());
  worker()->ExpectPendingCommits({kHash1, kHash1});

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
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  EXPECT_EQ(1U, ProcessorEntityCount());

  const EntityMetadata metadata_v3 = db().GetMetadata(kKey1);
  EXPECT_TRUE(metadata_v3.is_deleted());
  EXPECT_EQ(2, metadata_v3.sequence_number());
  EXPECT_EQ(1, metadata_v3.acked_sequence_number());
  EXPECT_EQ(1, metadata_v3.server_version());

  worker()->AckOnePendingCommit();
  // The delete was acked so the metadata should now be cleared.
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
}

TEST_F(SharedModelTypeProcessorTest, ServerDeleteItem) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(1U, db().MetadataCount());
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  worker()->TombstoneFromServer(kHash1);
  // Delete from server should clear the data and all the metadata.
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, LocalDeleteUnknown) {
  InitializeToReadyState();
  DeleteItem(kKey1);
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, ServerDeleteUnknown) {
  InitializeToReadyState();
  worker()->TombstoneFromServer(kHash1);
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(SharedModelTypeProcessorTest, TwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, worker()->GetNumPendingCommits());

  WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  const EntityMetadata metadata1 = db().GetMetadata(kKey1);

  // There should be one commit request for this item only.
  worker()->ExpectPendingCommits({kHash1});

  WriteItem(kKey2, kValue2);
  EXPECT_EQ(2U, db().DataCount());
  EXPECT_EQ(2U, db().MetadataCount());
  const EntityMetadata metadata2 = db().GetMetadata(kKey2);

  // The second write should trigger another single-item commit request.
  worker()->ExpectPendingCommits({kHash1, kHash2});

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
  EntitySpecifics specifics = WriteItem(kKey1, kValue1);
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(kValue1, db().GetValue(kKey1));
  EXPECT_EQ(1U, db().MetadataChangeCount());
  EXPECT_EQ(kUncommittedVersion, db().GetMetadata(kKey1).server_version());
  worker()->ExpectPendingCommits({kHash1});
  worker()->ExpectNthPendingCommit(0, kHash1, specifics);

  // Changes match doesn't call ResolveConflict.
  worker()->UpdateFromServer(kHash1, specifics);

  // Updated metadata but not data; no new commit request.
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->ExpectPendingCommits({kHash1});
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseLocal) {
  InitializeToReadyState();
  EntitySpecifics specifics = WriteItem(kKey1, kValue1);
  SetConflictResolution(ConflictResolution::UseLocal());

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));

  // Updated metadata but not data; new commit request.
  EXPECT_EQ(1U, db().DataChangeCount());
  EXPECT_EQ(2U, db().MetadataChangeCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->ExpectPendingCommits({kHash1, kHash1});
  worker()->ExpectNthPendingCommit(1, kHash1, specifics);
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseRemote) {
  InitializeToReadyState();
  WriteItem(kKey1, kValue1);
  SetConflictResolution(ConflictResolution::UseRemote());
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));

  // Updated client data and metadata; no new commit request.
  EXPECT_EQ(2U, db().DataChangeCount());
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().MetadataChangeCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->ExpectPendingCommits({kHash1});
}

TEST_F(SharedModelTypeProcessorTest, ConflictResolutionUseNew) {
  InitializeToReadyState();
  WriteItem(kKey1, kValue1);
  SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(2U, db().DataChangeCount());
  EXPECT_EQ(kValue3, db().GetValue(kKey1));
  EXPECT_EQ(2U, db().MetadataChangeCount());
  EXPECT_EQ(1, db().GetMetadata(kKey1).server_version());
  worker()->ExpectPendingCommits({kHash1, kHash1});
  worker()->ExpectNthPendingCommit(1, kHash1,
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
  WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  DisconnectSync();

  // The third item is added after stopping.
  WriteItem(kKey3, kValue3);

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
  WriteItem(kKey2, kValue2);
  EXPECT_TRUE(worker()->HasPendingCommitForHash(kHash2));

  DisableSync();

  // The third item is added after disable.
  WriteItem(kKey3, kValue3);

  // Now we re-enable.
  CreateChangeProcessor();
  OnMetadataLoaded();
  OnSyncStarting();
  OnInitialSyncDone();

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  worker()->ExpectPendingCommits({kHash1, kHash2, kHash3});
}

// Test re-encrypt everything when desired encryption key changes.
TEST_F(SharedModelTypeProcessorTest, ReEncryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);
  // Create another item and don't wait for its commit response.
  EntitySpecifics specifics2 = WriteItem(kKey2, kValue2);
  worker()->ExpectPendingCommits({kHash2});
  EXPECT_EQ(1U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(1U, db().GetMetadata(kKey2).sequence_number());

  // Receive notice that the account's desired encryption key has changed.
  worker()->UpdateWithEncryptionKey("k1");
  // Tag 2 is recommitted immediately because the data was in memory.
  ASSERT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash2, specifics2);
  // Sequence numbers in the store are updated.
  EXPECT_EQ(2U, db().GetMetadata(kKey1).sequence_number());
  EXPECT_EQ(2U, db().GetMetadata(kKey2).sequence_number());

  // Tag 1 needs to go to the store to load its data before recommitting.
  OnPendingCommitDataLoaded();
  ASSERT_EQ(3U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(2, kHash1, specifics1);
}

// Test that an error loading pending commit data for re-encryption is
// propagated to the error handler.
TEST_F(SharedModelTypeProcessorTest, ReEncryptErrorLoadingData) {
  InitializeToReadyState();
  WriteItemAndAck(kKey1, kValue1);
  SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  worker()->UpdateWithEncryptionKey("k1");
  error_handler()->ExpectError(syncer::SyncError::DATATYPE_ERROR);
  OnPendingCommitDataLoaded();
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
  worker()->ExpectPendingCommits({kHash2});
  OnPendingCommitDataLoaded();
  // kKey1 needed data so once that's loaded, it is also queued.
  worker()->ExpectPendingCommits({kHash2, kHash1});

  // Receive a separate update that was encrypted with key k1.
  worker()->UpdateFromServer(kHash4, GenerateSpecifics(kKey4, kValue1), 1,
                             "k1");
  // Receipt of updates encrypted with old key also forces a re-encrypt commit.
  worker()->ExpectPendingCommits({kHash2, kHash1, kHash4});

  // Receive an update that was encrypted with key k2.
  worker()->UpdateFromServer(kHash5, GenerateSpecifics(kKey5, kValue1), 1,
                             "k2");
  // That was the correct key, so no re-encryption is required.
  worker()->ExpectPendingCommits({kHash2, kHash1, kHash4});
}

// Test that re-encrypting enqueues the right data for USE_LOCAL conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseLocal) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  EntitySpecifics specifics = WriteItem(kKey1, kValue1);
  worker()->ExpectPendingCommits({kHash1});

  SetConflictResolution(ConflictResolution::UseLocal());
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash1, specifics);
  EXPECT_EQ(kValue1, db().GetValue(kKey1));
}

// Test that re-encrypting enqueues the right data for USE_REMOTE conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseRemote) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  WriteItem(kKey1, kValue1);

  SetConflictResolution(ConflictResolution::UseRemote());
  // Unencrypted update needs to be re-commited with key k1.
  EntitySpecifics specifics = GenerateSpecifics(kKey1, kValue2);
  worker()->UpdateFromServer(kHash1, specifics, 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash1, specifics);
  EXPECT_EQ(kValue2, db().GetValue(kKey1));
}

// Test that re-encrypting enqueues the right data for USE_NEW conflicts.
TEST_F(SharedModelTypeProcessorTest, ReEncryptConflictResolutionUseNew) {
  InitializeToReadyState();
  worker()->UpdateWithEncryptionKey("k1");
  WriteItem(kKey1, kValue1);

  SetConflictResolution(
      ConflictResolution::UseNew(GenerateEntityData(kKey1, kValue3)));
  // Unencrypted update needs to be re-commited with key k1.
  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2), 1, "");

  // Ensure the re-commit has the correct value.
  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash1,
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
  worker()->ExpectNthPendingCommit(0, kHash1, specifics);
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
  worker()->ExpectNthPendingCommit(0, kHash1, specifics);

  worker()->UpdateFromServer(kHash1, GenerateSpecifics(kKey1, kValue2));
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
}

// Tests that a real local change wins over a remote encryption-only change.
TEST_F(SharedModelTypeProcessorTest, IgnoreRemoteEncryption) {
  InitializeToReadyState();
  EntitySpecifics specifics1 = WriteItemAndAck(kKey1, kValue1);

  EntitySpecifics specifics2 = WriteItem(kKey1, kValue2);
  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash1, specifics2);
}

// Same as above but with two commit requests before one ack.
TEST_F(SharedModelTypeProcessorTest, IgnoreRemoteEncryptionInterleaved) {
  InitializeToReadyState();
  EntitySpecifics specifics1 = WriteItem(kKey1, kValue1);
  EntitySpecifics specifics2 = WriteItem(kKey1, kValue2);
  worker()->AckOnePendingCommit();
  // kValue1 is now the base value.
  EXPECT_EQ(1U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(0, kHash1, specifics2);

  UpdateResponseDataList update;
  update.push_back(worker()->GenerateUpdateData(kHash1, specifics1, 1, "k1"));
  worker()->UpdateWithEncryptionKey("k1", update);

  EXPECT_EQ(2U, worker()->GetNumPendingCommits());
  worker()->ExpectNthPendingCommit(1, kHash1, specifics2);
}

}  // namespace syncer_v2
