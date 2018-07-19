// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_service_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "components/sync/model_impl/blocking_model_type_store_impl.h"
#include "components/sync/model_impl/model_type_store_backend.h"
#include "components/sync/model_impl/model_type_store_impl.h"

namespace syncer {
namespace {

constexpr base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

constexpr base::FilePath::CharType kLevelDBFolderName[] =
    FILE_PATH_LITERAL("LevelDB");

// Initialized ModelTypeStoreBackend, on the backend sequence.
void InitOnBackendSequence(scoped_refptr<ModelTypeStoreBackend> store_backend,
                           const base::FilePath& level_db_path) {
  base::Optional<ModelError> error = store_backend->Init(level_db_path);
  if (error) {
    LOG(ERROR) << "Failed to initialize ModelTypeStore backend: "
               << error->ToString();
  }
}

// CreateBlockingModelTypeStoreOnBackendSequence() could return the store by
// value, but that's incompatible with binding the function with a WeakPtr
// for the reply.
void CreateBlockingModelTypeStoreOnBackendSequence(
    ModelType type,
    scoped_refptr<ModelTypeStoreBackend> store_backend,
    std::unique_ptr<BlockingModelTypeStoreImpl>* result) {
  result->reset();
  if (store_backend->IsInitialized()) {
    *result = std::make_unique<BlockingModelTypeStoreImpl>(type, store_backend);
  }
}

}  // namespace

ModelTypeStoreServiceImpl::ModelTypeStoreServiceImpl(
    const base::FilePath& base_path)
    : sync_path_(base_path.Append(base::FilePath(kSyncDataFolderName))),
      leveldb_path_(sync_path_.Append(base::FilePath(kLevelDBFolderName))),
      backend_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      store_backend_(ModelTypeStoreBackend::CreateUninitialized()),
      weak_ptr_factory_(this) {
  DCHECK(backend_task_runner_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InitOnBackendSequence, store_backend_, leveldb_path_));
}

ModelTypeStoreServiceImpl::~ModelTypeStoreServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::DoNothing::Once<scoped_refptr<ModelTypeStoreBackend>>(),
          std::move(store_backend_)));
}

const base::FilePath& ModelTypeStoreServiceImpl::GetSyncDataPath() const {
  return sync_path_;
}

RepeatingModelTypeStoreFactory ModelTypeStoreServiceImpl::GetStoreFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&ModelTypeStoreServiceImpl::CreateModelTypeStore,
                             weak_ptr_factory_.GetWeakPtr());
}

scoped_refptr<base::SequencedTaskRunner>
ModelTypeStoreServiceImpl::GetBackendTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return backend_task_runner_;
}

std::unique_ptr<BlockingModelTypeStore>
ModelTypeStoreServiceImpl::CreateBlockingStoreFromBackendSequence(
    ModelType type) {
  DCHECK(backend_task_runner_->RunsTasksInCurrentSequence());
  if (!store_backend_) {
    return nullptr;
  }
  return std::make_unique<BlockingModelTypeStoreImpl>(type, store_backend_);
}

void ModelTypeStoreServiceImpl::CreateModelTypeStore(
    ModelType type,
    ModelTypeStore::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  // BlockingModelTypeStoreImpl must be instantiated in the backend sequence.
  // This also guarantees that the creation is sequenced with the backend's
  // initialization, since we can't know for sure that InitOnBackendSequence()
  // has already run.

  // CreateBlockingModelTypeStoreOnBackendSequence() cannot return by value
  // weak_ptrs can only bind to methods without return values.
  auto blocking_store_ptr =
      std::make_unique<std::unique_ptr<BlockingModelTypeStoreImpl>>();

  base::OnceClosure task =
      base::BindOnce(&CreateBlockingModelTypeStoreOnBackendSequence, type,
                     store_backend_, blocking_store_ptr.get());

  base::OnceClosure reply =
      base::BindOnce(&ModelTypeStoreServiceImpl::CreateModelTypeStoreOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), type, std::move(callback),
                     std::move(blocking_store_ptr));

  backend_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                         std::move(reply));
}

void ModelTypeStoreServiceImpl::CreateModelTypeStoreOnUIThread(
    ModelType type,
    ModelTypeStore::InitCallback callback,
    std::unique_ptr<std::unique_ptr<BlockingModelTypeStoreImpl>>
        blocking_store_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  DCHECK(blocking_store_ptr);

  if (*blocking_store_ptr) {
    std::move(callback).Run(
        /*error=*/base::nullopt,
        std::make_unique<ModelTypeStoreImpl>(
            type, std::move(*blocking_store_ptr), backend_task_runner_));
  } else {
    std::move(callback).Run(
        ModelError(FROM_HERE, "ModelTypeStore backend initialization failed"),
        /*store=*/nullptr);
  }
}

}  // namespace syncer
