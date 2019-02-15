// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_selector.h"

#include <memory>
#include <utility>

#include "components/leveldb_proto/internal/migration_delegate.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/internal/unique_proto_database.h"

namespace leveldb_proto {

namespace {

void RunInitCallbackOnTaskRunner(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    Enums::InitStatus status) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), status));
}

}  // namespace

ProtoDatabaseSelector::ProtoDatabaseSelector(
    ProtoDbType db_type,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<SharedProtoDatabaseProvider> db_provider)
    : db_type_(db_type),
      task_runner_(task_runner),
      db_provider_(std::move(db_provider)),
      migration_delegate_(std::make_unique<MigrationDelegate>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoDatabaseSelector::~ProtoDatabaseSelector() {
  if (db_)
    task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
}

void ProtoDatabaseSelector::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    db_ = std::make_unique<UniqueProtoDatabase>(task_runner_);

  db_->InitWithDatabase(
      database, database_dir, options, false,
      base::BindOnce(&RunInitCallbackOnTaskRunner, std::move(callback),
                     callback_task_runner));
  OnInitDone();
}

void ProtoDatabaseSelector::InitUniqueOrShared(
    const std::string& client_name,
    base::FilePath db_dir,
    const leveldb_env::Options& options,
    bool use_shared_db,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_status_ = InitStatus::IN_PROGRESS;
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(db_dir, options, task_runner_);
  auto* unique_db_ptr = unique_db.get();
  unique_db_ptr->Init(
      client_name.c_str(),
      base::BindOnce(
          &ProtoDatabaseSelector::OnInitUniqueDB, this, std::move(unique_db),
          use_shared_db,
          base::BindOnce(&RunInitCallbackOnTaskRunner, std::move(callback),
                         callback_task_runner)));
}

void ProtoDatabaseSelector::OnInitUniqueDB(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the unique DB is corrupt, just return it early with the corruption
  // status to avoid silently migrating a corrupt database and giving no errors
  // back.
  if (status == Enums::InitStatus::kCorrupt) {
    db_ = std::move(unique_db);
    std::move(callback).Run(Enums::InitStatus::kCorrupt);
    OnInitDone();
    return;
  }

  // Clear out the unique_db before sending an unusable DB into InitSharedDB,
  // a nullptr indicates opening a unique DB failed.
  if (status != Enums::InitStatus::kOK)
    unique_db.reset();

  GetSharedDBClient(std::move(unique_db), use_shared_db, std::move(callback));
}

void ProtoDatabaseSelector::GetSharedDBClient(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_provider_) {
    OnInitSharedDB(std::move(unique_db), use_shared_db, std::move(callback),
                   nullptr);
    return;
  }
  // Get the current task runner to ensure the callback is run on the same
  // callback as the rest, and the WeakPtr checks out on the right sequence.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  db_provider_->GetDBInstance(
      base::BindOnce(&ProtoDatabaseSelector::OnInitSharedDB, this,
                     std::move(unique_db), use_shared_db, std::move(callback)),
      task_runner_);
}

void ProtoDatabaseSelector::OnInitSharedDB(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<SharedProtoDatabase> shared_db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_db) {
    // If we have a reference to the shared database, try to get a client.
    shared_db->GetClientAsync(
        db_type_, use_shared_db,
        base::BindOnce(&ProtoDatabaseSelector::OnGetSharedDBClient, this,
                       std::move(unique_db), use_shared_db,
                       std::move(callback)));
    return;
  }

  // Otherwise, we just call the OnGetSharedDBClient function with a nullptr
  // client.
  OnGetSharedDBClient(std::move(unique_db), use_shared_db, std::move(callback),
                      nullptr);
}

void ProtoDatabaseSelector::OnGetSharedDBClient(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    std::unique_ptr<SharedProtoDatabaseClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!unique_db && !client) {
    std::move(callback).Run(Enums::InitStatus::kError);
    OnInitDone();
    return;
  }

  if (!unique_db || !client) {
    // One of two things happened:
    // 1) We failed to get a shared DB instance, so regardless of whether we
    // want to use the shared DB or not, we'll settle for the unique DB.
    // 2) We failed to initialize a unique DB, but got  access to the shared DB,
    // so we use that regardless of whether we "should be" or not.
    db_ = client ? std::move(client) : std::move(unique_db);
    std::move(callback).Run(Enums::InitStatus::kOK);
    OnInitDone();
    return;
  }

  UniqueProtoDatabase* from = nullptr;
  UniqueProtoDatabase* to = nullptr;
  if (use_shared_db) {
    switch (client->migration_status()) {
      case SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED:
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL:
        from = unique_db.get();
        to = client.get();
        break;
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL:
        // Unique db was deleted in previous migration, so nothing to do here.
        return OnMigrationCleanupComplete(std::move(unique_db),
                                          std::move(client), use_shared_db,
                                          std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED:
        // Migration transfer was completed, so just try deleting the unique db.
        return OnMigrationTransferComplete(std::move(unique_db),
                                           std::move(client), use_shared_db,
                                           std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED:
        // Shared db was not deleted in last migration and we want to use shared
        // db. So, delete stale data, and attempt migration.
        return DeleteOldDataAndMigrate(std::move(unique_db), std::move(client),
                                       use_shared_db, std::move(callback));
    }
  } else {
    switch (client->migration_status()) {
      case SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED:
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL:
        from = client.get();
        to = unique_db.get();
        break;
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED:
        // Unique db was not deleted in last migration and we want to use unique
        // db. So, delete stale data, and attempt migration.
        return DeleteOldDataAndMigrate(std::move(unique_db), std::move(client),
                                       use_shared_db, std::move(callback));
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL:
        // Shared db was deleted in previous migration, so nothing to do here.
        return OnMigrationCleanupComplete(std::move(unique_db),
                                          std::move(client), use_shared_db,
                                          std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED:
        // Migration transfer was completed, so just try deleting the shared db.
        return OnMigrationTransferComplete(std::move(unique_db),
                                           std::move(client), use_shared_db,
                                           std::move(callback), true);
    }
  }

  // We got access to both the unique DB and a shared DB, meaning we need to
  // attempt migration and give back the right one.
  migration_delegate_->DoMigration(
      from, to,
      base::BindOnce(&ProtoDatabaseSelector::OnMigrationTransferComplete, this,
                     std::move(unique_db), std::move(client), use_shared_db,
                     std::move(callback)));
}

void ProtoDatabaseSelector::DeleteOldDataAndMigrate(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase* to_remove_old_data =
      use_shared_db ? client.get() : unique_db.get();
  auto maybe_do_migration =
      base::BindOnce(&ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld,
                     this, std::move(unique_db), std::move(client),
                     std::move(callback), use_shared_db);

  to_remove_old_data->UpdateEntriesWithRemoveFilter(
      std::make_unique<KeyValueVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      std::move(maybe_do_migration));
}

void ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    Callbacks::InitStatusCallback callback,
    bool use_shared_db,
    bool delete_success) {
  if (!delete_success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Old data has not been removed from the database we want to use. We also
    // know that previous attempt of migration failed for same reason. Give up
    // on this database and use the other.
    // This update is not necessary since this was the old value. But update to
    // be clear.
    client->UpdateClientInitMetadata(
        use_shared_db
            ? SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED
            : SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);
    db_ = use_shared_db ? std::move(unique_db) : std::move(client);
    std::move(callback).Run(Enums::InitStatus::kOK);
    OnInitDone();
    return;
  }

  auto* from = use_shared_db ? unique_db.get() : client.get();
  auto* to = use_shared_db ? client.get() : unique_db.get();
  migration_delegate_->DoMigration(
      from, to,
      base::BindOnce(&ProtoDatabaseSelector::OnMigrationTransferComplete, this,
                     std::move(unique_db), std::move(client), use_shared_db,
                     std::move(callback)));
}

void ProtoDatabaseSelector::OnMigrationTransferComplete(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    // Call Destroy on the DB we no longer want to use.
    auto* db_destroy_ptr = use_shared_db ? unique_db.get() : client.get();
    db_destroy_ptr->Destroy(
        base::BindOnce(&ProtoDatabaseSelector::OnMigrationCleanupComplete, this,
                       std::move(unique_db), std::move(client), use_shared_db,
                       std::move(callback)));
    return;
  }

  // Failing to transfer the old data means that the requested database to be
  // used could have some bad data. So, mark them to be deleted before use in
  // the next runs.
  client->UpdateClientInitMetadata(
      use_shared_db
          ? SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED
          : SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);
  db_ = use_shared_db ? std::move(unique_db) : std::move(client);
  std::move(callback).Run(Enums::InitStatus::kOK);
  OnInitDone();
}

void ProtoDatabaseSelector::OnMigrationCleanupComplete(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We still return true in our callback below because we do have a database as
  // far as the original caller is concerned. As long as |db_| is assigned, we
  // return true.
  if (success) {
    client->UpdateClientInitMetadata(
        use_shared_db ? SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL
                      : SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL);
  } else {
    client->UpdateClientInitMetadata(
        use_shared_db
            ? SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED
            : SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);
  }
  // Migration transfer was complete. So, we should use the requested database.
  db_ = use_shared_db ? std::move(client) : std::move(unique_db);
  std::move(callback).Run(Enums::InitStatus::kOK);
  OnInitDone();
}

void ProtoDatabaseSelector::AddTransaction(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (init_status_) {
    case InitStatus::IN_PROGRESS:
      if (pending_tasks_.size() > 10) {
        std::move(pending_tasks_.front()).Run();
        pending_tasks_.pop();
      }
      pending_tasks_.push(std::move(task));
      break;
    case InitStatus::NOT_STARTED:
      NOTREACHED();
      FALLTHROUGH;
    case InitStatus::DONE:
      std::move(task).Run();
      break;
  }
}

void ProtoDatabaseSelector::UpdateEntries(
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_status_, InitStatus::DONE);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     std::move(callback));
}

void ProtoDatabaseSelector::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                     delete_key_filter, target_prefix,
                                     std::move(callback));
}

void ProtoDatabaseSelector::LoadEntries(
    typename Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadEntries(std::move(callback));
}

void ProtoDatabaseSelector::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadEntriesWithFilter(key_filter, options, target_prefix,
                             std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntries(
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntries(std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntriesWithFilter(filter, options, target_prefix,
                                    std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntriesInRange(start, end, std::move(callback));
}

void ProtoDatabaseSelector::LoadKeys(const std::string& target_prefix,
                                     Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeys(target_prefix, std::move(callback));
}

void ProtoDatabaseSelector::GetEntry(const std::string& key,
                                     typename Callbacks::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->GetEntry(key, std::move(callback));
}

void ProtoDatabaseSelector::Destroy(Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->Destroy(std::move(callback));
}

void ProtoDatabaseSelector::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->RemoveKeysForTesting(key_filter, target_prefix, std::move(callback));
}

void ProtoDatabaseSelector::OnInitDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_status_ = InitStatus::DONE;
  while (!pending_tasks_.empty()) {
    task_runner_->PostTask(FROM_HERE, std::move(pending_tasks_.front()));
    pending_tasks_.pop();
  }
}

}  // namespace leveldb_proto
