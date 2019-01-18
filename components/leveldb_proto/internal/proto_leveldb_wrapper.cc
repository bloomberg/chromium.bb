// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

namespace {

Enums::InitStatus InitFromTaskRunner(LevelDB* database,
                                     const base::FilePath& database_dir,
                                     const leveldb_env::Options& options,
                                     bool destroy_on_corruption,
                                     const std::string& client_id) {
  // TODO(cjhopman): Histogram for database size.
  auto status = database->Init(database_dir, options, destroy_on_corruption);
  ProtoLevelDBWrapperMetrics::RecordInit(client_id, status);

  if (status.ok())
    return Enums::InitStatus::kOK;
  if (status.IsCorruption())
    return Enums::InitStatus::kCorrupt;
  if (status.IsNotSupportedError() || status.IsInvalidArgument())
    return Enums::InitStatus::kInvalidOperation;
  return Enums::InitStatus::kError;
}

bool DestroyFromTaskRunner(LevelDB* database, const std::string& client_id) {
  auto status = database->Destroy();
  bool success = status.ok();
  ProtoLevelDBWrapperMetrics::RecordDestroy(client_id, success);

  return success;
}

void LoadKeysFromTaskRunner(
    LevelDB* database,
    const std::string& target_prefix,
    const std::string& client_id,
    Callbacks::LoadKeysCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  auto keys = std::make_unique<std::vector<std::string>>();
  bool success = database->LoadKeys(target_prefix, keys.get());
  ProtoLevelDBWrapperMetrics::RecordLoadKeys(client_id, success);
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, std::move(keys)));
}

void RemoveKeysFromTaskRunner(
    LevelDB* database,
    const std::string& target_prefix,
    const KeyFilter& filter,
    const std::string& client_id,
    Callbacks::UpdateCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  leveldb::Status status;
  bool success = database->UpdateWithRemoveFilter(base::StringPairs(), filter,
                                                  target_prefix, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}

}  // namespace

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    LevelDB* db)
    : task_runner_(task_runner), db_(db), weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::~ProtoLevelDBWrapper() = default;

void ProtoLevelDBWrapper::RunInitCallback(Callbacks::InitCallback callback,
                                          const leveldb::Status* status) {
  std::move(callback).Run(status->ok());
}

void ProtoLevelDBWrapper::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    bool destroy_on_corruption,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(database);
  db_ = database;

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(InitFromTaskRunner, base::Unretained(db_), database_dir,
                     options, destroy_on_corruption, metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeys(
    typename Callbacks::LoadKeysCallback callback) {
  LoadKeys(std::string(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeys(
    const std::string& target_prefix,
    typename Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(LoadKeysFromTaskRunner, base::Unretained(db_),
                                target_prefix, metrics_id_, std::move(callback),
                                base::SequencedTaskRunnerHandle::Get()));
}

void ProtoLevelDBWrapper::RemoveKeys(const KeyFilter& filter,
                                     const std::string& target_prefix,
                                     Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(RemoveKeysFromTaskRunner, base::Unretained(db_),
                     target_prefix, filter, metrics_id_, std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

void ProtoLevelDBWrapper::Destroy(Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(DestroyFromTaskRunner, base::Unretained(db_), metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::SetMetricsId(const std::string& id) {
  metrics_id_ = id;
}

bool ProtoLevelDBWrapper::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  if (!db_)
    return 0;

  return db_->GetApproximateMemoryUse(approx_mem_use);
}

const scoped_refptr<base::SequencedTaskRunner>&
ProtoLevelDBWrapper::task_runner() {
  return task_runner_;
}

}  // namespace leveldb_proto