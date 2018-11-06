// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_leveldb_wrapper.h"

#include "components/leveldb_proto/proto_leveldb_wrapper_metrics.h"

namespace leveldb_proto {

namespace {

void RunInitCallback(typename ProtoLevelDBWrapper::InitCallback callback,
                     const leveldb::Status* status) {
  std::move(callback).Run(status->ok());
}

inline void InitFromTaskRunner(LevelDB* database,
                               const base::FilePath& database_dir,
                               const leveldb_env::Options& options,
                               bool destroy_on_corruption,
                               leveldb::Status* status,
                               const std::string& client_id) {
  DCHECK(status);

  // TODO(cjhopman): Histogram for database size.
  *status = database->Init(database_dir, options, destroy_on_corruption);
  ProtoLevelDBWrapperMetrics::RecordInit(client_id, *status);
}

void RunDestroyCallback(typename ProtoLevelDBWrapper::DestroyCallback callback,
                        const bool* success) {
  std::move(callback).Run(*success);
}

inline void DestroyFromTaskRunner(LevelDB* database,
                                  bool* success,
                                  const std::string& client_id) {
  CHECK(success);

  auto status = database->Destroy();
  *success = status.ok();
  ProtoLevelDBWrapperMetrics::RecordDestroy(client_id, *success);
}

void RunLoadKeysCallback(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback,
    std::unique_ptr<bool> success,
    std::unique_ptr<std::vector<std::string>> keys) {
  std::move(callback).Run(*success, std::move(keys));
}

inline void LoadKeysFromTaskRunner(LevelDB* database,
                                   std::vector<std::string>* keys,
                                   bool* success,
                                   const std::string& client_id) {
  DCHECK(success);
  DCHECK(keys);
  keys->clear();
  *success = database->LoadKeys(keys);
  ProtoLevelDBWrapperMetrics::RecordLoadKeys(client_id, *success);
}

}  // namespace

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

ProtoLevelDBWrapper::~ProtoLevelDBWrapper() = default;

void ProtoLevelDBWrapper::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    typename ProtoLevelDBWrapper::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!db_);
  DCHECK(database);
  db_ = database;
  leveldb::Status* status = new leveldb::Status();
  // Sets |destroy_on_corruption| to true to mimic the original behaviour for
  // now.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(InitFromTaskRunner, base::Unretained(db_), database_dir,
                     options, true /* destroy_on_corruption */, status,
                     metrics_id_),
      base::BindOnce(RunInitCallback, std::move(callback),
                     base::Owned(status)));
}

void ProtoLevelDBWrapper::Destroy(
    typename ProtoLevelDBWrapper::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(db_);

  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(DestroyFromTaskRunner, base::Unretained(db_), success,
                     metrics_id_),
      base::BindOnce(RunDestroyCallback, std::move(callback),
                     base::Owned(success)));
}

void ProtoLevelDBWrapper::LoadKeys(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto success = std::make_unique<bool>(false);
  auto keys = std::make_unique<std::vector<std::string>>();
  auto load_task = base::BindOnce(LoadKeysFromTaskRunner, base::Unretained(db_),
                                  keys.get(), success.get(), metrics_id_);
  task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(load_task),
      base::BindOnce(RunLoadKeysCallback, std::move(callback),
                     std::move(success), std::move(keys)));
}

void ProtoLevelDBWrapper::SetMetricsId(const std::string& id) {
  metrics_id_ = id;
}

bool ProtoLevelDBWrapper::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  return db_->GetApproximateMemoryUse(approx_mem_use);
}

const scoped_refptr<base::SequencedTaskRunner>&
ProtoLevelDBWrapper::task_runner() {
  return task_runner_;
}

}  // namespace leveldb_proto