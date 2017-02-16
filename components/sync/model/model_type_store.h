// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

// ModelTypeStore is leveldb backed store for model type's data, metadata and
// global metadata.
//
// Store keeps records for entries identified by ids. For each entry store keeps
// data and metadata. Also store keeps one record for global metadata.
//
// To create store call one of Create*Store static factory functions. Model type
// controls store's lifetime with returned unique_ptr. Call to Create*Store
// function triggers asynchronous store backend initialization, callback will be
// called with results when initialization is done.
//
// Read operations are asynchronous, initiated with one of Read* functions,
// provided callback will be called with result code and output of read
// operation.
//
// Write operations are done in context of write batch. To get one call
// CreateWriteBatch(). After that pass write batch object to Write/Delete
// functions. WriteBatch only accumulates pending changes, doesn't actually do
// data modification. Calling CommitWriteBatch writes all accumulated changes to
// disk atomically. Callback passed to CommitWriteBatch will be called with
// result of write operation. If write batch object is destroyed without
// comitting accumulated write operations will not be persisted.
//
// Destroying store object doesn't necessarily cancel asynchronous operations
// issued previously. You should be prepared to handle callbacks from those
// operations.
class ModelTypeStore {
 public:
  // Result of store operations.
  enum class Result {
    SUCCESS,
    UNSPECIFIED_ERROR,
    SCHEMA_VERSION_TOO_HIGH,
  };

  // Output of read operations is passed back as list of Record structures.
  struct Record {
    Record(const std::string& id, const std::string& value)
        : id(id), value(value) {}

    std::string id;
    std::string value;
  };

  // WriteBatch object is used in all modification operations.
  class WriteBatch {
   public:
    // Creates a MetadataChangeList that will accumulate metadata changes and
    // can later be passed to a WriteBatch via TransferChanges. Use this when
    // you need a MetadataChangeList and do not have a WriteBatch in scope.
    static std::unique_ptr<MetadataChangeList> CreateMetadataChangeList();

    virtual ~WriteBatch();

    // Write the given |value| for data with |id|.
    void WriteData(const std::string& id, const std::string& value);

    // Delete the record for data with |id|.
    void DeleteData(const std::string& id);

    // Provides access to a MetadataChangeList that will pass its changes
    // directly into this WriteBatch. Use this when you need a
    // MetadataChangeList and already have a WriteBatch in scope.
    MetadataChangeList* GetMetadataChangeList();

    // Transfers the changes from a MetadataChangeList into this WriteBatch.
    // |mcl| must have previously been created by CreateMetadataChangeList().
    void TransferMetadataChanges(std::unique_ptr<MetadataChangeList> mcl);

   protected:
    friend class MockModelTypeStore;
    explicit WriteBatch(ModelTypeStore* store);

   private:
    // A pointer to the store that generated this WriteBatch.
    ModelTypeStore* store_;

    // A MetadataChangeList that is being used to pass changes directly into the
    // WriteBatch. Only accessible via GetMetadataChangeList(), and not created
    // unless necessary.
    std::unique_ptr<MetadataChangeList> metadata_change_list_;
  };

  using RecordList = std::vector<Record>;
  using IdList = std::vector<std::string>;

  using InitCallback =
      base::Callback<void(Result result,
                          std::unique_ptr<ModelTypeStore> store)>;
  using CallbackWithResult = base::Callback<void(Result result)>;
  using ReadDataCallback =
      base::Callback<void(Result result,
                          std::unique_ptr<RecordList> data_records,
                          std::unique_ptr<IdList> missing_id_list)>;
  using ReadAllDataCallback =
      base::Callback<void(Result result,
                          std::unique_ptr<RecordList> data_records)>;
  using ReadMetadataCallback =
      base::Callback<void(base::Optional<ModelError> error,
                          std::unique_ptr<MetadataBatch> metadata_batch)>;

  // CreateStore takes |path| and |blocking_task_runner|. Here is how to get
  // task runner in production code:
  //
  // base::SequencedWorkerPool* worker_pool =
  //     content::BrowserThread::GetBlockingPool();
  // scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
  //     worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
  //         worker_pool->GetSequenceToken(),
  //         base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  //
  // In test get task runner from MessageLoop::task_runner().
  static void CreateStore(
      ModelType type,
      const std::string& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const InitCallback& callback);
  // Creates store object backed by in-memory leveldb database. It is used in
  // tests.
  static void CreateInMemoryStoreForTest(ModelType type,
                                         const InitCallback& callback);

  virtual ~ModelTypeStore();

  // Read operations return records either for all entries or only for ones
  // identified in |id_list|. Result is SUCCESS if all records were read
  // successfully. If reading any of records fails result is UNSPECIFIED_ERROR
  // and RecordList contains some records that were read successfully. There is
  // no guarantee that RecordList will contain all successfully read records in
  // this case.
  // Callback for ReadData (ReadDataCallback) in addition receives list of ids
  // that were not found in store (missing_id_list).
  virtual void ReadData(const IdList& id_list,
                        const ReadDataCallback& callback) = 0;
  virtual void ReadAllData(const ReadAllDataCallback& callback) = 0;
  // ReadMetadataCallback will be invoked with three parameters: result of
  // operation, list of metadata records and global metadata.
  virtual void ReadAllMetadata(const ReadMetadataCallback& callback) = 0;

  // Creates write batch for write operations.
  virtual std::unique_ptr<WriteBatch> CreateWriteBatch() = 0;

  // Commits write operations accumulated in write batch. If write operation
  // fails result is UNSPECIFIED_ERROR and write operations will not be
  // reflected in the store.
  virtual void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                                const CallbackWithResult& callback) = 0;

 protected:
  friend class AccumulatingMetadataChangeList;
  friend class ModelTypeStoreImplTest;
  friend class PassthroughMetadataChangeList;

  // Write operations; access via WriteBatch.
  virtual void WriteData(WriteBatch* write_batch,
                         const std::string& id,
                         const std::string& value) = 0;
  virtual void WriteMetadata(WriteBatch* write_batch,
                             const std::string& id,
                             const std::string& value) = 0;
  virtual void WriteGlobalMetadata(WriteBatch* write_batch,
                                   const std::string& value) = 0;
  virtual void DeleteData(WriteBatch* write_batch, const std::string& id) = 0;
  virtual void DeleteMetadata(WriteBatch* write_batch,
                              const std::string& id) = 0;
  virtual void DeleteGlobalMetadata(WriteBatch* write_batch) = 0;
  // TODO(pavely): Consider implementing DeleteAllMetadata with following
  // signature:
  // virtual void DeleteAllMetadata(const CallbackWithResult& callback) = 0.
  // It will delete all metadata records and global metadata record.
};

// Typedef for a store factory that has all params bound except InitCallback.
using ModelTypeStoreFactory =
    base::Callback<void(const ModelTypeStore::InitCallback&)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_H_
