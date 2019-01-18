// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database.h"

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto/shared_db_metadata.pb.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

namespace {

const char kMetadataDatabaseName[] = "metadata";
const base::FilePath::CharType kMetadataDatabasePath[] =
    FILE_PATH_LITERAL("metadata");
const int kMaxInitMetaDatabaseAttempts = 3;

const char kGlobalMetadataKey[] = "__global";

}  // namespace

// static
const base::TimeDelta SharedProtoDatabase::kDelayToClearObsoleteDatabase =
    base::TimeDelta::FromSeconds(120);

inline void RunInitStatusCallbackOnCallingSequence(
    SharedProtoDatabase::SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status,
    SharedDBMetadataProto::MigrationStatus migration_status) {
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, migration_status));
}

SharedProtoDatabase::InitRequest::InitRequest(
    SharedClientInitCallback callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::string& client_db_id)
    : callback(std::move(callback)),
      task_runner(std::move(task_runner)),
      client_db_id(client_db_id) {}

SharedProtoDatabase::InitRequest::~InitRequest() = default;

SharedProtoDatabase::SharedProtoDatabase(const std::string& client_db_id,
                                         const base::FilePath& db_dir)
    : task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      db_dir_(db_dir),
      db_(std::make_unique<LevelDB>(client_db_id.c_str())),
      db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      metadata_db_(std::make_unique<LevelDB>(kMetadataDatabaseName)),
      metadata_db_wrapper_(
          std::make_unique<ProtoLevelDBWrapper>(task_runner_)) {
  DETACH_FROM_SEQUENCE(on_task_runner_);
}

// All init functionality runs on the same SequencedTaskRunner, so any caller of
// this after a database Init will receive the correct status of the database.
// PostTaskAndReply is used to ensure that we call the Init callback on its
// original calling thread.
void SharedProtoDatabase::GetDatabaseInitStatusAsync(
    const std::string& client_db_id,
    Callbacks::InitStatusCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SharedProtoDatabase::RunInitCallback, this,
                                std::move(callback),
                                base::SequencedTaskRunnerHandle::Get()));
}

void SharedProtoDatabase::RunInitCallback(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), init_status_));
}

void SharedProtoDatabase::UpdateClientMetadataAsync(
    const std::string& client_db_id,
    SharedDBMetadataProto::MigrationStatus migration_status,
    base::OnceCallback<void(bool)> callback) {
  if (base::SequencedTaskRunnerHandle::Get() != task_runner_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedProtoDatabase::UpdateClientMetadataAsync, this,
                       client_db_id, migration_status, std::move(callback)));
    return;
  }
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();
  SharedDBMetadataProto write_proto;
  write_proto.set_corruptions(metadata_->corruptions());
  write_proto.set_migration_status(migration_status);
  update_entries->emplace_back(
      std::make_pair(std::string(client_db_id), write_proto));

  metadata_db_wrapper_->UpdateEntries<SharedDBMetadataProto>(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      std::move(callback));
}

void SharedProtoDatabase::GetClientMetadataAsync(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // |metadata_db_wrapper_| uses the same TaskRunner as Init and the main
  // DB, so making this call directly here without PostTasking is safe. In
  // addition, GetEntry uses PostTaskAndReply so the callback will be triggered
  // on the calling sequence.
  metadata_db_wrapper_->GetEntry<SharedDBMetadataProto>(
      std::string(client_db_id),
      base::BindOnce(&SharedProtoDatabase::OnGetClientMetadata, this,
                     client_db_id, std::move(callback),
                     std::move(callback_task_runner)));
}

// As mentioned above, |current_task_runner| is the appropriate calling sequence
// for the callback since the GetEntry call in GetClientMetadataAsync uses
// PostTaskAndReply.
void SharedProtoDatabase::OnGetClientMetadata(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  // If fetching metadata failed, then ignore the error.
  if (!success) {
    RunInitStatusCallbackOnCallingSequence(
        std::move(callback), std::move(callback_task_runner),
        Enums::InitStatus::kOK, SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);
    return;
  }
  if (!proto || !proto->has_migration_status()) {
    UpdateClientMetadataAsync(
        client_db_id, SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
        base::BindOnce(
            [](SharedClientInitCallback callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
               bool update_success) {
              // Do not care about update success since next time we will reset
              // corruption and migration status to 0.
              RunInitStatusCallbackOnCallingSequence(
                  std::move(callback), std::move(callback_task_runner),
                  Enums::InitStatus::kOK,
                  SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);
            },
            std::move(callback), std::move(callback_task_runner)));
    return;
  }
  // If we've made it here, we know that the current status of our database is
  // OK. Make it return corrupt if the metadata disagrees.
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner),
      metadata_->corruptions() != proto->corruptions()
          ? Enums::InitStatus::kCorrupt
          : Enums::InitStatus::kOK,
      proto->migration_status());
}

void SharedProtoDatabase::CheckCorruptionAndRunInitCallback(
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
  if (init_status_ == Enums::InitStatus::kOK) {
    GetClientMetadataAsync(client_db_id, std::move(callback),
                           std::move(callback_task_runner));
    return;
  }
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner), init_status_,
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);
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
    const std::string& client_db_id,
    SharedClientInitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // If we succeeded previously, just check for corruption status and run init
  // callback.
  if (init_state_ == InitState::kSuccess) {
    CheckCorruptionAndRunInitCallback(client_db_id, std::move(callback),
                                      std::move(callback_task_runner),
                                      Enums::InitStatus::kOK);
    return;
  }

  outstanding_init_requests_.emplace(std::make_unique<InitRequest>(
      std::move(callback), std::move(callback_task_runner), client_db_id));
  if (init_state_ == InitState::kInProgress)
    return;

  init_state_ = InitState::kInProgress;
  // First, try to initialize the metadata database.
  InitMetadataDatabase(create_if_missing, 0 /* attempt */,
                       false /* corruption */);
}

void SharedProtoDatabase::ProcessInitRequests(Enums::InitStatus status) {
  DCHECK(!outstanding_init_requests_.empty());

  // The pairs are stored as (callback, callback_task_runner).
  while (!outstanding_init_requests_.empty()) {
    auto request = std::move(outstanding_init_requests_.front());
    CheckCorruptionAndRunInitCallback(request->client_db_id,
                                      std::move(request->callback),
                                      std::move(request->task_runner), status);
    outstanding_init_requests_.pop();
  }
}

// We allow some number of attempts to be made to initialize the metadata
// database because it's crucial for the operation of the shared database. In
// the event that the metadata DB is corrupt, at least one retry will be made
// so that we create the DB from scratch again.
// |corruption| lets us know whether the retries are because of corruption.
void SharedProtoDatabase::InitMetadataDatabase(bool create_shared_db_if_missing,
                                               int attempt,
                                               bool corruption) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  if (attempt >= kMaxInitMetaDatabaseAttempts) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    ProcessInitRequests(init_status_);
    return;
  }

  base::FilePath metadata_path =
      db_dir_.Append(base::FilePath(kMetadataDatabasePath));
  metadata_db_wrapper_->InitWithDatabase(
      metadata_db_.get(), metadata_path, CreateSimpleOptions(),
      true /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnMetadataInitComplete, this,
                     create_shared_db_if_missing, attempt, corruption));
}

void SharedProtoDatabase::OnMetadataInitComplete(
    bool create_shared_db_if_missing,
    int attempt,
    bool corruption,
    Enums::InitStatus metadata_init_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  if (metadata_init_status == Enums::InitStatus::kCorrupt) {
    // Retry InitMetaDatabase to create the metadata database from scratch.
    InitMetadataDatabase(create_shared_db_if_missing, ++attempt,
                         true /* corruption */);
    return;
  }

  if (metadata_init_status != Enums::InitStatus::kOK) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    ProcessInitRequests(init_status_);
    return;
  }

  // Read or initialize the corruption count for this DB. If |corruption| is
  // true, we initialize the counter to 1 right away so that all DBs are forced
  // to treat the shared database as corrupt, we can't know for sure anymore.
  metadata_db_wrapper_->GetEntry<SharedDBMetadataProto>(
      std::string(kGlobalMetadataKey),
      base::BindOnce(&SharedProtoDatabase::OnGetGlobalMetadata, this,
                     create_shared_db_if_missing, corruption));
}

void SharedProtoDatabase::OnGetGlobalMetadata(
    bool create_shared_db_if_missing,
    bool corruption,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  if (success && proto) {
    // It existed so let's update our internal |corruption_count_|
    metadata_ = std::move(proto);
    InitDatabase(create_shared_db_if_missing);
    return;
  }

  // We failed to get the global metadata, so we need to create it for the first
  // time.
  metadata_.reset(new SharedDBMetadataProto());
  metadata_->set_corruptions(corruption ? 1U : 0U);
  metadata_->clear_migration_status();
  CommitUpdatedGlobalMetadata(
      base::BindOnce(&SharedProtoDatabase::OnFinishCorruptionCountWrite, this,
                     create_shared_db_if_missing));
}

void SharedProtoDatabase::OnFinishCorruptionCountWrite(
    bool create_shared_db_if_missing,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  // TODO(thildebr): Should we retry a few times if we fail this? It feels like
  // if we fail to write this single value something serious happened with the
  // metadata database.
  if (!success) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    ProcessInitRequests(init_status_);
    return;
  }

  InitDatabase(create_shared_db_if_missing);
}

void SharedProtoDatabase::InitDatabase(bool create_shared_db_if_missing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  auto options = CreateSimpleOptions();
  options.create_if_missing = create_shared_db_if_missing;
  // |db_wrapper_| uses the same SequencedTaskRunner that Init is called on,
  // so OnDatabaseInit will be called on the same sequence after Init.
  // This means any callers to Init using the same TaskRunner can guarantee that
  // the InitState will be final after Init is called.
  db_wrapper_->InitWithDatabase(
      db_.get(), db_dir_, options, false /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnDatabaseInit, this));
}

void SharedProtoDatabase::OnDatabaseInit(Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  // Update the corruption counter locally and in the database.
  if (status == Enums::InitStatus::kCorrupt) {
    // TODO(thildebr): Find a clean way to break this into a function to be used
    // by OnGetCorruptionCountEntry.
    // If this fails, we actually send back a failure to the init callback.
    // This may be overkill, but what happens if we don't update this value?
    // Again, it seems like a failure to update here will indicate something
    // serious has gone wrong with the metadata database.
    metadata_->set_corruptions(metadata_->corruptions() + 1);

    CommitUpdatedGlobalMetadata(base::BindOnce(
        &SharedProtoDatabase::OnUpdateCorruptionCountAtInit, this));
    return;
  }

  init_status_ = status;
  init_state_ = status == Enums::InitStatus::kOK ? InitState::kSuccess
                                                 : InitState::kFailure;
  ProcessInitRequests(status);

  if (init_state_ == InitState::kSuccess) {
    // Create a ProtoLevelDBWrapper just like we create for each client, for
    // deleting data from obsolete clients. It is fine to use the same wrapper
    // to clear data from all clients. This object will be destroyed after
    // clearing data for all these clients.
    auto db_wrapper =
        std::make_unique<ProtoLevelDBWrapper>(task_runner_, db_.get());
    Callbacks::UpdateCallback obsolete_cleared_callback = base::DoNothing();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DestroyObsoleteSharedProtoDatabaseClients,
                       std::move(db_wrapper),
                       std::move(obsolete_cleared_callback)),
        kDelayToClearObsoleteDatabase);
  }
}

void SharedProtoDatabase::OnUpdateCorruptionCountAtInit(bool success) {
  // Return the success value of our write to update the corruption counter.
  // This means that we return kError when the update fails, as a safeguard
  // against clients trying to further persist data when something's gone
  // wrong loading a single metadata proto.
  init_state_ = success ? InitState::kSuccess : InitState::kFailure;
  init_status_ =
      success ? Enums::InitStatus::kCorrupt : Enums::InitStatus::kError;
  ProcessInitRequests(init_status_);
}

void SharedProtoDatabase::CommitUpdatedGlobalMetadata(
    Callbacks::UpdateCallback callback) {
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();

  SharedDBMetadataProto write_proto;
  write_proto.CheckTypeAndMergeFrom(*metadata_);
  update_entries->emplace_back(
      std::make_pair(std::string(kGlobalMetadataKey), write_proto));
  metadata_db_wrapper_->UpdateEntries<SharedDBMetadataProto>(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      std::move(callback));
}

SharedProtoDatabase::~SharedProtoDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
  task_runner_->DeleteSoon(FROM_HERE, std::move(metadata_db_));
}

LevelDB* SharedProtoDatabase::GetLevelDBForTesting() const {
  return db_.get();
}

}  // namespace leveldb_proto