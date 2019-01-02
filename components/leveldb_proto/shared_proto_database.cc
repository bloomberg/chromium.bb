// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database.h"

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

inline void RunCallbackOnCallingSequence(
    Callbacks::InitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success) {
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}

SharedProtoDatabase::SharedProtoDatabase(const std::string& client_name,
                                         const base::FilePath& db_dir)
    : task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      db_dir_(db_dir),
      db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      db_(std::make_unique<LevelDB>(client_name.c_str())),
      weak_factory_(
          std::make_unique<base::WeakPtrFactory<SharedProtoDatabase>>(this)) {
  DETACH_FROM_SEQUENCE(on_task_runner_);
}

// All init functionality runs on the same SequencedTaskRunner, so any caller of
// this after a database Init will receive the correct status of the database.
// PostTaskAndReply is used to ensure that we call the Init callback on its
// original calling thread.
void SharedProtoDatabase::GetDatabaseInitStatusAsync(
    Callbacks::InitStatusCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&SharedProtoDatabase::RunInitCallback,
                     weak_factory_->GetWeakPtr(), std::move(callback)));
}

void SharedProtoDatabase::RunInitCallback(
    Callbacks::InitStatusCallback callback) {
  std::move(callback).Run(init_status_);
}

// Setting |create_if_missing| to false allows us to test whether or not the
// shared database already exists, useful for migrating data from the shared
// database to a unique database if it exists.
// All clients planning to use the shared database should be setting
// |create_if_missing| to true. Setting this to false can result in unexpected
// behaviour since the ordering of Init calls may matter if some calls are made
// with this set to true, and others false.
void SharedProtoDatabase::Init(
    bool create_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // If we succeeded previously, just let the callback know. Otherwise, we'll
  // continue to try initialization for every new request.
  if (init_state_ == InitState::kSuccess) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  Enums::InitStatus::kOK /* status */));
    return;
  }

  if (init_state_ == InitState::kInProgress) {
    outstanding_init_requests_.emplace(
        std::make_pair(std::move(callback), std::move(callback_task_runner)));
    return;
  }

  init_state_ = InitState::kInProgress;
  auto options = CreateSimpleOptions();
  options.create_if_missing = create_if_missing;
  // |db_wrapper_| uses the same SequencedTaskRunner that Init is called on,
  // so OnDatabaseInit will be called on the same sequence after Init.
  // This means any callers to Init using the same TaskRunner can guarantee that
  // the InitState will be final after Init is called.
  db_wrapper_->InitWithDatabase(
      db_.get(), db_dir_, options, false /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnDatabaseInit,
                     weak_factory_->GetWeakPtr(), std::move(callback),
                     std::move(callback_task_runner)));
}

void SharedProtoDatabase::ProcessInitRequests(Enums::InitStatus status) {
  // The pairs are stored as (callback, callback_task_runner).
  while (!outstanding_init_requests_.empty()) {
    auto request = std::move(outstanding_init_requests_.front());
    auto task_runner = std::move(request.second);
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(request.first), status));
    outstanding_init_requests_.pop();
  }
}

void SharedProtoDatabase::OnDatabaseInit(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  init_state_ = status == Enums::InitStatus::kOK ? InitState::kSuccess
                                                 : InitState::kFailure;

  ProcessInitRequests(status);
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
}

SharedProtoDatabase::~SharedProtoDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(weak_factory_));
}

LevelDB* SharedProtoDatabase::GetLevelDBForTesting() const {
  return db_.get();
}

}  // namespace leveldb_proto