// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model_impl/model_type_store_backend.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer {

namespace {

// Key prefix for data/metadata records.
const char kDataPrefix[] = "-dt-";
const char kMetadataPrefix[] = "-md-";

// Key for global metadata record.
const char kGlobalMetadataKey[] = "-GlobalMetadata";

void NoOpForBackendDtor(scoped_refptr<ModelTypeStoreBackend> backend) {
  // This function was intentionally left blank.
}

// Formats key prefix for data records of |type|.
std::string FormatDataPrefix(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kDataPrefix;
}

// Formats key prefix for metadata records of |type|.
std::string FormatMetaPrefix(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kMetadataPrefix;
}

// Formats key for global metadata record of |type|.
std::string FormatGlobalMetadataKey(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kGlobalMetadataKey;
}

}  // namespace

// static
leveldb::WriteBatch* ModelTypeStoreImpl::GetLeveldbWriteBatch(
    WriteBatch* write_batch) {
  return static_cast<WriteBatchImpl*>(write_batch)->leveldb_write_batch_.get();
}

std::string ModelTypeStoreImpl::FormatDataKey(const std::string& id) {
  return data_prefix_ + id;
}

std::string ModelTypeStoreImpl::FormatMetadataKey(const std::string& id) {
  return metadata_prefix_ + id;
}

ModelTypeStoreImpl::ModelTypeStoreImpl(
    ModelType type,
    scoped_refptr<ModelTypeStoreBackend> backend,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_(backend),
      backend_task_runner_(backend_task_runner),
      data_prefix_(FormatDataPrefix(type)),
      metadata_prefix_(FormatMetaPrefix(type)),
      global_metadata_key_(FormatGlobalMetadataKey(type)),
      weak_ptr_factory_(this) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_);
}

ModelTypeStoreImpl::~ModelTypeStoreImpl() {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NoOpForBackendDtor, base::Passed(&backend_)));
}

// static
void ModelTypeStoreImpl::CreateStore(
    ModelType type,
    const std::string& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const InitCallback& callback) {
  DCHECK(!callback.is_null());
  std::unique_ptr<leveldb::Env> env;
  std::unique_ptr<Result> result(new Result());
  auto task = base::Bind(&ModelTypeStoreBackend::GetOrCreateBackend, path,
                         base::Passed(&env), result.get());
  auto reply =
      base::Bind(&ModelTypeStoreImpl::BackendInitDone, type,
                 base::Passed(&result), blocking_task_runner, callback);

  base::PostTaskAndReplyWithResult(blocking_task_runner.get(), FROM_HERE, task,
                                   reply);
}

// static
void ModelTypeStoreImpl::CreateInMemoryStoreForTest(
    ModelType type,
    const InitCallback& callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<leveldb::Env> env =
      ModelTypeStoreBackend::CreateInMemoryEnv();

  std::string path;
  env->GetTestDirectory(&path);
  path += "/in-memory";

  // In-memory store backend works on the same thread as test.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  std::unique_ptr<Result> result(new Result());

  auto task = base::Bind(&ModelTypeStoreBackend::GetOrCreateBackend, path,
                         base::Passed(&env), result.get());
  auto reply = base::Bind(&ModelTypeStoreImpl::BackendInitDone, type,
                          base::Passed(&result), task_runner, callback);

  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE, task, reply);
}

// static
void ModelTypeStoreImpl::BackendInitDone(
    ModelType type,
    std::unique_ptr<Result> result,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const InitCallback& callback,
    scoped_refptr<ModelTypeStoreBackend> backend) {
  std::unique_ptr<ModelTypeStoreImpl> store;
  if (*result == Result::SUCCESS) {
    store.reset(new ModelTypeStoreImpl(type, backend, blocking_task_runner));
  }

  callback.Run(*result, std::move(store));
}

// Note on pattern for communicating with backend:
//  - API function (e.g. ReadData) allocates lists for output.
//  - API function prepares two callbacks: task that will be posted on backend
//    thread and reply which will be posted on model type thread once task
//    finishes.
//  - Task for backend thread takes raw pointers to output lists while reply
//    takes ownership of those lists. This allows backend interface to be simple
//    while ensuring proper objects' lifetime.
//  - Function bound by reply calls consumer's callback and passes ownership of
//    output lists to it.

void ModelTypeStoreImpl::ReadData(const IdList& id_list,
                                  const ReadDataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());

  auto task = base::Bind(&ModelTypeStoreBackend::ReadRecordsWithPrefix,
                         base::Unretained(backend_.get()), data_prefix_,
                         id_list, base::Unretained(record_list.get()),
                         base::Unretained(missing_id_list.get()));
  auto reply = base::Bind(
      &ModelTypeStoreImpl::ReadDataDone, weak_ptr_factory_.GetWeakPtr(),
      callback, base::Passed(&record_list), base::Passed(&missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadDataDone(const ReadDataCallback& callback,
                                      std::unique_ptr<RecordList> record_list,
                                      std::unique_ptr<IdList> missing_id_list,
                                      Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result, std::move(record_list), std::move(missing_id_list));
}

void ModelTypeStoreImpl::ReadAllData(const ReadAllDataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadAllRecordsWithPrefix,
                         base::Unretained(backend_.get()), data_prefix_,
                         base::Unretained(record_list.get()));
  auto reply = base::Bind(&ModelTypeStoreImpl::ReadAllDataDone,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          base::Passed(&record_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadAllDataDone(
    const ReadAllDataCallback& callback,
    std::unique_ptr<RecordList> record_list,
    Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result, std::move(record_list));
}

void ModelTypeStoreImpl::ReadAllMetadata(const ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  // ReadAllMetadata performs two steps sequentially: read all metadata records
  // and then read global metadata record. Start reading metadata records here.
  // When this read operation is done ReadMetadataRecordsDone callback will
  // issue read operation for global metadata record.
  std::unique_ptr<RecordList> metadata_records(new RecordList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadAllRecordsWithPrefix,
                         base::Unretained(backend_.get()), metadata_prefix_,
                         base::Unretained(metadata_records.get()));
  auto reply = base::Bind(&ModelTypeStoreImpl::ReadMetadataRecordsDone,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          base::Passed(&metadata_records));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadMetadataRecordsDone(
    const ReadMetadataCallback& callback,
    std::unique_ptr<RecordList> metadata_records,
    Result result) {
  DCHECK(CalledOnValidThread());
  if (result != Result::SUCCESS) {
    callback.Run(ModelError(FROM_HERE, "Reading metadata failed."),
                 base::MakeUnique<MetadataBatch>());
    return;
  }

  IdList global_metadata_id;
  global_metadata_id.push_back(global_metadata_key_);
  std::unique_ptr<RecordList> global_metadata_records(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadRecordsWithPrefix,
                         base::Unretained(backend_.get()), std::string(),
                         global_metadata_id,
                         base::Unretained(global_metadata_records.get()),
                         base::Unretained(missing_id_list.get()));
  auto reply = base::Bind(
      &ModelTypeStoreImpl::ReadAllMetadataDone, weak_ptr_factory_.GetWeakPtr(),
      callback, base::Passed(&metadata_records),
      base::Passed(&global_metadata_records), base::Passed(&missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadAllMetadataDone(
    const ReadMetadataCallback& callback,
    std::unique_ptr<RecordList> metadata_records,
    std::unique_ptr<RecordList> global_metadata_records,
    std::unique_ptr<IdList> missing_id_list,
    Result result) {
  DCHECK(CalledOnValidThread());

  if (result != Result::SUCCESS) {
    callback.Run(ModelError(FROM_HERE, "Reading metadata failed."),
                 base::MakeUnique<MetadataBatch>());
    return;
  }

  std::string global_metadata = "";
  if (!missing_id_list->empty()) {
    // Missing global metadata record is not an error; we can just return the
    // default instance using the empty string above.
    DCHECK_EQ(global_metadata_key_, (*missing_id_list)[0]);
    DCHECK(global_metadata_records->empty());
  } else {
    DCHECK_EQ(1U, global_metadata_records->size());
    DCHECK_EQ(global_metadata_key_, (*global_metadata_records)[0].id);
    global_metadata = (*global_metadata_records)[0].value;
  }

  DeserializeMetadata(callback, global_metadata, std::move(metadata_records));
}

void ModelTypeStoreImpl::DeserializeMetadata(
    const ReadMetadataCallback& callback,
    const std::string& global_metadata,
    std::unique_ptr<RecordList> metadata_records) {
  auto metadata_batch = base::MakeUnique<MetadataBatch>();

  sync_pb::ModelTypeState state;
  if (!state.ParseFromString(global_metadata)) {
    callback.Run(
        ModelError(FROM_HERE, "Failed to deserialize model type state."),
        base::MakeUnique<MetadataBatch>());
    return;
  }
  metadata_batch->SetModelTypeState(state);

  for (const Record& r : *metadata_records.get()) {
    sync_pb::EntityMetadata entity_metadata;
    if (!entity_metadata.ParseFromString(r.value)) {
      callback.Run(
          ModelError(FROM_HERE, "Failed to deserialize entity metadata."),
          base::MakeUnique<MetadataBatch>());
      return;
    }
    metadata_batch->AddMetadata(r.id, entity_metadata);
  }

  callback.Run({}, std::move(metadata_batch));
}

std::unique_ptr<ModelTypeStore::WriteBatch>
ModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK(CalledOnValidThread());
  return base::MakeUnique<WriteBatchImpl>(this);
}

void ModelTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    const CallbackWithResult& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  WriteBatchImpl* write_batch_impl =
      static_cast<WriteBatchImpl*>(write_batch.get());
  auto task = base::Bind(&ModelTypeStoreBackend::WriteModifications,
                         base::Unretained(backend_.get()),
                         base::Passed(&write_batch_impl->leveldb_write_batch_));
  auto reply = base::Bind(&ModelTypeStoreImpl::WriteModificationsDone,
                          weak_ptr_factory_.GetWeakPtr(), callback);
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::WriteModificationsDone(
    const CallbackWithResult& callback,
    Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result);
}

void ModelTypeStoreImpl::WriteData(WriteBatch* write_batch,
                                   const std::string& id,
                                   const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(FormatDataKey(id), value);
}

void ModelTypeStoreImpl::WriteMetadata(WriteBatch* write_batch,
                                       const std::string& id,
                                       const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(FormatMetadataKey(id), value);
}

void ModelTypeStoreImpl::WriteGlobalMetadata(WriteBatch* write_batch,
                                             const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(global_metadata_key_, value);
}

void ModelTypeStoreImpl::DeleteData(WriteBatch* write_batch,
                                    const std::string& id) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(FormatDataKey(id));
}

void ModelTypeStoreImpl::DeleteMetadata(WriteBatch* write_batch,
                                        const std::string& id) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(FormatMetadataKey(id));
}

void ModelTypeStoreImpl::DeleteGlobalMetadata(WriteBatch* write_batch) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(global_metadata_key_);
}

ModelTypeStoreImpl::WriteBatchImpl::WriteBatchImpl(ModelTypeStore* store)
    : WriteBatch(store) {
  leveldb_write_batch_ = base::MakeUnique<leveldb::WriteBatch>();
}

ModelTypeStoreImpl::WriteBatchImpl::~WriteBatchImpl() {}

}  // namespace syncer
