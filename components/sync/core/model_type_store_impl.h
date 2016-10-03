// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_MODEL_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_CORE_MODEL_TYPE_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/api/model_type_store.h"
#include "components/sync/base/model_type.h"

namespace leveldb {
class WriteBatch;
}  // namespace leveldb

namespace syncer {

class ModelTypeStoreBackend;

// ModelTypeStoreImpl handles details of store initialization, threading and
// leveldb key formatting. Actual leveldb IO calls are performed by
// ModelTypeStoreBackend.
class ModelTypeStoreImpl : public ModelTypeStore, public base::NonThreadSafe {
 public:
  ~ModelTypeStoreImpl() override;

  static void CreateStore(
      const ModelType type,
      const std::string& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const InitCallback& callback);
  static void CreateInMemoryStoreForTest(const InitCallback& callback);

  // ModelTypeStore implementation.
  void ReadData(const IdList& id_list,
                const ReadDataCallback& callback) override;
  void ReadAllData(const ReadAllDataCallback& callback) override;
  void ReadAllMetadata(const ReadMetadataCallback& callback) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        const CallbackWithResult& callback) override;
  void WriteData(WriteBatch* write_batch,
                 const std::string& id,
                 const std::string& value) override;
  void WriteMetadata(WriteBatch* write_batch,
                     const std::string& id,
                     const std::string& value) override;
  void WriteGlobalMetadata(WriteBatch* write_batch,
                           const std::string& value) override;
  void DeleteData(WriteBatch* write_batch, const std::string& id) override;
  void DeleteMetadata(WriteBatch* write_batch, const std::string& id) override;
  void DeleteGlobalMetadata(WriteBatch* write_batch) override;

 private:
  class WriteBatchImpl : public WriteBatch {
   public:
    WriteBatchImpl();
    ~WriteBatchImpl() override;
    std::unique_ptr<leveldb::WriteBatch> leveldb_write_batch_;
  };

  static void BackendInitDone(
      const ModelType type,
      std::unique_ptr<Result> result,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const InitCallback& callback,
      scoped_refptr<ModelTypeStoreBackend> backend);

  // Format prefix key for data/metadata records with |type|.
  static std::string FormatDataPrefix(const ModelType type);
  static std::string FormatMetaPrefix(const ModelType type);

  static leveldb::WriteBatch* GetLeveldbWriteBatch(WriteBatch* write_batch);

  // Format key for data/metadata records with given id.
  std::string FormatDataKey(const std::string& id);
  std::string FormatMetadataKey(const std::string& id);

  ModelTypeStoreImpl(
      const ModelType type,
      scoped_refptr<ModelTypeStoreBackend> backend,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  // Callbacks for different calls to ModelTypeStoreBackend.
  void ReadDataDone(const ReadDataCallback& callback,
                    std::unique_ptr<RecordList> record_list,
                    std::unique_ptr<IdList> missing_id_list,
                    Result result);
  void ReadAllDataDone(const ReadAllDataCallback& callback,
                       std::unique_ptr<RecordList> record_list,
                       Result result);
  void ReadMetadataRecordsDone(const ReadMetadataCallback& callback,
                               std::unique_ptr<RecordList> metadata_records,
                               Result result);
  void ReadAllMetadataDone(const ReadMetadataCallback& callback,
                           std::unique_ptr<RecordList> metadata_records,
                           std::unique_ptr<RecordList> global_metadata_records,
                           std::unique_ptr<IdList> missing_id_list,
                           Result result);
  void WriteModificationsDone(const CallbackWithResult& callback,
                              Result result);

  // Backend should be deleted on backend thread.
  // To accomplish this store's dtor posts task to backend thread passing
  // backend ownership to task parameter.
  scoped_refptr<ModelTypeStoreBackend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Key prefix for data/metadata records of this model type.
  const std::string data_prefix_;
  const std::string metadata_prefix_;

  base::WeakPtrFactory<ModelTypeStoreImpl> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_MODEL_TYPE_STORE_IMPL_H_
