// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database.h"

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto/shared_db_metadata.pb.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

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
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
}

SharedProtoDatabase::InitRequest::InitRequest(
    Callbacks::InitStatusCallback callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::string& client_name)
    : callback(std::move(callback)),
      task_runner(std::move(task_runner)),
      client_name(client_name) {}

SharedProtoDatabase::InitRequest::~InitRequest() = default;

SharedProtoDatabase::SharedProtoDatabase(const std::string& client_name,
                                         const base::FilePath& db_dir)
    : task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      db_dir_(db_dir),
      db_(std::make_unique<LevelDB>(client_name.c_str())),
      db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      metadata_db_(std::make_unique<LevelDB>(kMetadataDatabaseName)),
      metadata_db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      weak_factory_(
          std::make_unique<base::WeakPtrFactory<SharedProtoDatabase>>(this)) {
  DETACH_FROM_SEQUENCE(on_task_runner_);
}

// All init functionality runs on the same SequencedTaskRunner, so any caller of
// this after a database Init will receive the correct status of the database.
// PostTaskAndReply is used to ensure that we call the Init callback on its
// original calling thread.
void SharedProtoDatabase::GetDatabaseInitStatusAsync(
    const std::string& client_name,
    Callbacks::InitStatusCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&SharedProtoDatabase::CheckCorruptionAndRunInitCallback,
                     weak_factory_->GetWeakPtr(), client_name,
                     std::move(callback), std::move(current_task_runner),
                     init_status_));
}

// Should be called when processing client init requests after a corruption.
void SharedProtoDatabase::UpdateClientCorruptAsync(
    const std::string& client_name,
    base::OnceCallback<void(bool)> callback) {
  metadata_db_wrapper_->GetEntry<SharedDBMetadataProto>(
      std::string(client_name),
      base::BindOnce(&SharedProtoDatabase::OnGetClientMetadataForUpdate,
                     weak_factory_->GetWeakPtr(), client_name,
                     std::move(callback)));
}

void SharedProtoDatabase::GetClientCorruptAsync(
    const std::string& client_name,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // |metadata_db_wrapper_| uses the same TaskRunner as Init and the main
  // DB, so making this call directly here without PostTasking is safe. In
  // addition, GetEntry uses PostTaskAndReply so the callback will be triggered
  // on the calling sequence.
  metadata_db_wrapper_->GetEntry<SharedDBMetadataProto>(
      std::string(client_name),
      base::BindOnce(&SharedProtoDatabase::OnGetClientMetadata,
                     weak_factory_->GetWeakPtr(), std::move(callback),
                     std::move(callback_task_runner)));
}

// As mentioned above, |current_task_runner| is the appropriate calling sequence
// for the callback since the GetEntry call in GetClientCorruptAsync uses
// PostTaskAndReply.
void SharedProtoDatabase::OnGetClientMetadata(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  // If we've made it here, we know that the current status of our database is
  // OK. Make it return corrupt if the metadata disagrees.
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                metadata_->corruptions() != proto->corruptions()
                                    ? Enums::InitStatus::kCorrupt
                                    : Enums::InitStatus::kOK));
}

void SharedProtoDatabase::OnGetClientMetadataForUpdate(
    const std::string& client_name,
    base::OnceCallback<void(bool)> callback,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();
  SharedDBMetadataProto write_proto;
  write_proto.CheckTypeAndMergeFrom(*proto);
  write_proto.set_corruptions(metadata_->corruptions());
  update_entries->emplace_back(
      std::make_pair(std::string(client_name), write_proto));

  metadata_db_wrapper_->UpdateEntries<SharedDBMetadataProto>(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      std::move(callback));
}

void SharedProtoDatabase::CheckCorruptionAndRunInitCallback(
    const std::string& client_name,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
  if (init_status_ == Enums::InitStatus::kOK) {
    GetClientCorruptAsync(client_name, std::move(callback),
                          std::move(callback_task_runner));
    return;
  }
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner), init_status_);
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
    const std::string& client_name,
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
    outstanding_init_requests_.emplace(std::make_unique<InitRequest>(
        std::move(callback), std::move(callback_task_runner), client_name));
    return;
  }

  init_state_ = InitState::kInProgress;
  // First, try to initialize the metadata database.
  InitMetadataDatabase(create_if_missing, std::move(callback),
                       std::move(callback_task_runner), 0 /* attempt */,
                       false /* corruption */);
}

void SharedProtoDatabase::ProcessOutstandingInitRequests(
    Enums::InitStatus status) {
  // The pairs are stored as (callback, callback_task_runner).
  while (!outstanding_init_requests_.empty()) {
    auto request = std::move(outstanding_init_requests_.front());
    CheckCorruptionAndRunInitCallback(request->client_name,
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
void SharedProtoDatabase::InitMetadataDatabase(
    bool create_shared_db_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    int attempt,
    bool corruption) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  if (attempt >= kMaxInitMetaDatabaseAttempts) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), init_status_));
    ProcessOutstandingInitRequests(init_status_);
    return;
  }

  base::FilePath metadata_path =
      db_dir_.Append(base::FilePath(kMetadataDatabasePath));
  metadata_db_wrapper_->InitWithDatabase(
      metadata_db_.get(), metadata_path, CreateSimpleOptions(),
      true /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnMetadataInitComplete,
                     weak_factory_->GetWeakPtr(), create_shared_db_if_missing,
                     std::move(callback), std::move(callback_task_runner),
                     attempt, corruption));
}

void SharedProtoDatabase::OnMetadataInitComplete(
    bool create_shared_db_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    int attempt,
    bool corruption,
    Enums::InitStatus metadata_init_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  if (metadata_init_status == Enums::InitStatus::kCorrupt) {
    // Retry InitMetaDatabase to create the metadata database from scratch.
    InitMetadataDatabase(create_shared_db_if_missing, std::move(callback),
                         std::move(callback_task_runner), ++attempt,
                         true /* corruption */);
    return;
  }

  if (metadata_init_status != Enums::InitStatus::kOK) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    RunInitStatusCallbackOnCallingSequence(
        std::move(callback), std::move(callback_task_runner), init_status_);
    ProcessOutstandingInitRequests(init_status_);
    return;
  }

  // Read or initialize the corruption count for this DB. If |corruption| is
  // true, we initialize the counter to 1 right away so that all DBs are forced
  // to treat the shared database as corrupt, we can't know for sure anymore.
  metadata_db_wrapper_->GetEntry<SharedDBMetadataProto>(
      std::string(kGlobalMetadataKey),
      base::BindOnce(&SharedProtoDatabase::OnGetGlobalMetadata,
                     weak_factory_->GetWeakPtr(), create_shared_db_if_missing,
                     std::move(callback), std::move(callback_task_runner),
                     corruption));
}

void SharedProtoDatabase::OnGetGlobalMetadata(
    bool create_shared_db_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool corruption,
    bool success,
    std::unique_ptr<SharedDBMetadataProto> proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  if (success && proto) {
    // It existed so let's update our internal |corruption_count_|
    metadata_ = std::move(proto);
    InitDatabase(create_shared_db_if_missing, std::move(callback),
                 std::move(callback_task_runner));
    return;
  }

  // We failed to get the global metadata, so we need to create it for the first
  // time.
  metadata_.reset(new SharedDBMetadataProto());
  metadata_->set_corruptions(corruption ? 1U : 0U);
  auto update_entries = std::make_unique<
      std::vector<std::pair<std::string, SharedDBMetadataProto>>>();

  SharedDBMetadataProto write_proto;
  write_proto.CheckTypeAndMergeFrom(*metadata_);
  update_entries->emplace_back(
      std::make_pair(std::string(kGlobalMetadataKey), write_proto));
  metadata_db_wrapper_->UpdateEntries<SharedDBMetadataProto>(
      std::move(update_entries), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&SharedProtoDatabase::OnFinishCorruptionCountWrite,
                     weak_factory_->GetWeakPtr(), create_shared_db_if_missing,
                     std::move(callback), std::move(callback_task_runner)));
}

void SharedProtoDatabase::OnFinishCorruptionCountWrite(
    bool create_shared_db_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  // TODO(thildebr): Should we retry a few times if we fail this? It feels like
  // if we fail to write this single value something serious happened with the
  // metadata database.
  if (!success) {
    init_state_ = InitState::kFailure;
    init_status_ = Enums::InitStatus::kError;
    RunInitStatusCallbackOnCallingSequence(
        std::move(callback), std::move(callback_task_runner), init_status_);
    ProcessOutstandingInitRequests(init_status_);
    return;
  }

  InitDatabase(create_shared_db_if_missing, std::move(callback),
               std::move(callback_task_runner));
}

void SharedProtoDatabase::InitDatabase(
    bool create_shared_db_if_missing,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  auto options = CreateSimpleOptions();
  options.create_if_missing = create_shared_db_if_missing;
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

void SharedProtoDatabase::OnUpdateCorruptionCount(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success) {
  // Return the success value of our write to update the corruption counter.
  // This means that we return kError when the update fails, as a safeguard
  // against clients trying to further persist data when something's gone
  // wrong loading a single metadata proto.
  init_state_ = success ? InitState::kSuccess : InitState::kFailure;
  init_status_ =
      success ? Enums::InitStatus::kCorrupt : Enums::InitStatus::kError;
  RunInitStatusCallbackOnCallingSequence(
      std::move(callback), std::move(callback_task_runner), init_status_);
  ProcessOutstandingInitRequests(init_status_);
}

void SharedProtoDatabase::OnDatabaseInit(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Enums::InitStatus status) {
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
    auto update_entries = std::make_unique<
        std::vector<std::pair<std::string, SharedDBMetadataProto>>>();

    SharedDBMetadataProto write_proto;
    write_proto.CheckTypeAndMergeFrom(*metadata_);
    update_entries->emplace_back(
        std::make_pair(std::string(kGlobalMetadataKey), write_proto));
    metadata_db_wrapper_->UpdateEntries<SharedDBMetadataProto>(
        std::move(update_entries), std::make_unique<std::vector<std::string>>(),
        base::BindOnce(&SharedProtoDatabase::OnUpdateCorruptionCount,
                       weak_factory_->GetWeakPtr(), std::move(callback),
                       std::move(callback_task_runner)));
    return;
  }

  init_status_ = status;
  init_state_ = status == Enums::InitStatus::kOK ? InitState::kSuccess
                                                 : InitState::kFailure;

  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
  ProcessOutstandingInitRequests(status);

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

SharedProtoDatabase::~SharedProtoDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(weak_factory_));
}

LevelDB* SharedProtoDatabase::GetLevelDBForTesting() const {
  return db_.get();
}

}  // namespace leveldb_proto