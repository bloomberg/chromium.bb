// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model_impl/blocking_model_type_store_impl.h"

namespace syncer {

namespace {

// Holds a one to one mapping between profile path and SequencedTaskRunner. This
// class is expected to be accessed on any thread, and uses a lock to guarantee
// thread safety. The task runners are held onto by scoped_refptrs, and since
// this class is leaky, none of these task runners are ever destroyed.
class TaskRunnerMap {
 public:
  TaskRunnerMap() = default;

  scoped_refptr<base::SequencedTaskRunner> GetOrCreateTaskRunner(
      const std::string& path) {
    base::AutoLock scoped_lock(lock_);
    auto iter = task_runner_map_.find(path);
    if (iter == task_runner_map_.end()) {
      scoped_refptr<base::SequencedTaskRunner> task_runner =
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
      task_runner_map_[path] = task_runner;
      return task_runner;
    } else {
      return iter->second;
    }
  }

 private:
  mutable base::Lock lock_;
  std::map<std::string, scoped_refptr<base::SequencedTaskRunner>>
      task_runner_map_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunnerMap);
};

base::LazyInstance<TaskRunnerMap>::Leaky task_runner_map_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ModelTypeStoreImpl::ModelTypeStoreImpl(
    ModelType type,
    std::unique_ptr<BlockingModelTypeStoreImpl> backend_store,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : type_(type),
      backend_store_(std::move(backend_store)),
      backend_task_runner_(backend_task_runner),
      weak_ptr_factory_(this) {
  DCHECK(backend_store_);
  DCHECK(backend_task_runner_);
}

ModelTypeStoreImpl::~ModelTypeStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(pkasting): For some reason, using ReleaseSoon() here (which would be
  // more idiomatic) breaks tests... perhaps the non-nestable task that posts
  // runs later than this nestable one, and violates some assumption?
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::DoNothing::Once<std::unique_ptr<BlockingModelTypeStoreImpl>>(),
          std::move(backend_store_)));
}

// static
void ModelTypeStoreImpl::CreateStore(ModelType type,
                                     const std::string& path,
                                     InitCallback callback) {
  DCHECK(!callback.is_null());
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      task_runner_map_singleton.Get().GetOrCreateTaskRunner(path);
  auto error = std::make_unique<base::Optional<ModelError>>();
  auto task = base::BindOnce(&BlockingModelTypeStoreImpl::CreateStore, type,
                             path, error.get());
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::BackendInitDone, type,
                     std::move(error), task_runner, std::move(callback));
  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

// static
void ModelTypeStoreImpl::CreateInMemoryStoreForTest(ModelType type,
                                                    InitCallback callback) {
  DCHECK(!callback.is_null());

  // In-memory store backend works on the same sequence as test.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();
  DCHECK(task_runner);

  auto error = std::make_unique<base::Optional<ModelError>>();
  auto task = base::BindOnce(
      &BlockingModelTypeStoreImpl::CreateInMemoryStoreForTest, type);
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::BackendInitDone, type,
                     std::move(error), task_runner, std::move(callback));

  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

// static
void ModelTypeStoreImpl::BackendInitDone(
    ModelType type,
    std::unique_ptr<base::Optional<ModelError>> error,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    InitCallback callback,
    std::unique_ptr<BlockingModelTypeStoreImpl> backend) {
  std::unique_ptr<ModelTypeStoreImpl> store;
  if (!error->has_value()) {
    store.reset(
        new ModelTypeStoreImpl(type, std::move(backend), backend_task_runner));
  }

  std::move(callback).Run(*error, std::move(store));
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
                                  ReadDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());

  auto task = base::BindOnce(&BlockingModelTypeStore::ReadData,
                             base::Unretained(backend_store_.get()), id_list,
                             base::Unretained(record_list.get()),
                             base::Unretained(missing_id_list.get()));
  auto reply = base::BindOnce(
      &ModelTypeStoreImpl::ReadDataDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(record_list), std::move(missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadDataDone(ReadDataCallback callback,
                                      std::unique_ptr<RecordList> record_list,
                                      std::unique_ptr<IdList> missing_id_list,
                                      const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list),
                          std::move(missing_id_list));
}

void ModelTypeStoreImpl::ReadAllData(ReadAllDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  auto task = base::BindOnce(&BlockingModelTypeStore::ReadAllData,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(record_list.get()));
  auto reply = base::BindOnce(&ModelTypeStoreImpl::ReadAllDataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(record_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadAllDataDone(
    ReadAllDataCallback callback,
    std::unique_ptr<RecordList> record_list,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list));
}

void ModelTypeStoreImpl::ReadAllMetadata(ReadMetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  auto metadata_batch = std::make_unique<MetadataBatch>();
  auto task = base::BindOnce(&BlockingModelTypeStore::ReadAllMetadata,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(metadata_batch.get()));
  auto reply = base::BindOnce(&ModelTypeStoreImpl::ReadAllMetadataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(metadata_batch));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadAllMetadataDone(
    ReadMetadataCallback callback,
    std::unique_ptr<MetadataBatch> metadata_batch,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    std::move(callback).Run(*error, std::make_unique<MetadataBatch>());
    return;
  }

  std::move(callback).Run({}, std::move(metadata_batch));
}

void ModelTypeStoreImpl::DeleteAllDataAndMetadata(CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingModelTypeStore::DeleteAllDataAndMetadata,
                             base::Unretained(backend_store_.get()));
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

std::unique_ptr<ModelTypeStore::WriteBatch>
ModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BlockingModelTypeStoreImpl::CreateWriteBatchForType(type_);
}

void ModelTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingModelTypeStore::CommitWriteBatch,
                             base::Unretained(backend_store_.get()),
                             std::move(write_batch));
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::WriteModificationsDone(
    CallbackWithResult callback,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error);
}

}  // namespace syncer
