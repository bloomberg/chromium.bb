// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_WRAPPER_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_WRAPPER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/migration_delegate.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/shared_proto_database.h"
#include "components/leveldb_proto/shared_proto_database_client_list.h"
#include "components/leveldb_proto/shared_proto_database_provider.h"
#include "components/leveldb_proto/unique_proto_database.h"

namespace leveldb_proto {

class ProtoDatabaseProvider;

// Avoids circular dependencies between ProtoDatabaseWrapper and
// ProtoDatabaseProvider, since we'd need to include the provider header here
// to use |db_provider_|'s GetSharedDBInstance.
void GetSharedDBInstance(
    ProtoDatabaseProvider* db_provider,
    base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)> callback);

// The ProtoDatabaseWrapper<T> owns a ProtoDatabase<T> instance, and allows the
// underlying ProtoDatabase<T> implementation to change without users of the
// wrapper needing to know.
// This allows clients to request a DB instance without knowing whether or not
// it's a UniqueDatabase<T> or a SharedProtoDatabaseClient<T>.
//
// TODO: Discuss the init flow/migration path for unique/shared DB here.
template <typename T>
class ProtoDatabaseWrapper : public UniqueProtoDatabase<T> {
 public:
  ProtoDatabaseWrapper(
      const std::string& client_namespace,
      const std::string& type_prefix,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<SharedProtoDatabaseProvider> db_provider);

  virtual ~ProtoDatabaseWrapper() = default;

  // This version of Init is for compatibility, since many of the current
  // proto database still use this.
  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const leveldb_env::Options& options,
            Callbacks::InitCallback callback) override;

  void Init(const std::string& client_name,
            Callbacks::InitStatusCallback callback) override;

  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        Callbacks::InitStatusCallback callback) override;

  void UpdateEntries(std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
                         entries_to_save,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     Callbacks::UpdateCallback callback) override;

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback) override;

  void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback) override;

  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  void RunCallbackOnCallingSequence(base::OnceClosure callback);

 private:
  friend class ProtoDatabaseWrapperTest;

  void Init(const std::string& client_name,
            bool use_shared_db,
            Callbacks::InitStatusCallback callback);

  virtual void InitUniqueDB(const std::string& client_name,
                            const leveldb_env::Options& options,
                            bool use_shared_db,
                            Callbacks::InitStatusCallback callback);
  void OnInitUniqueDB(std::unique_ptr<ProtoDatabase<T>> db,
                      bool use_shared_db,
                      Callbacks::InitStatusCallback callback,
                      Enums::InitStatus status);

  // |unique_db| should contain a nullptr if initializing the DB fails.
  void GetSharedDBClient(std::unique_ptr<ProtoDatabase<T>> unique_db,
                         bool use_shared_db,
                         Callbacks::InitStatusCallback callback);
  void OnInitSharedDB(std::unique_ptr<ProtoDatabase<T>> unique_db,
                      bool use_shared_db,
                      Callbacks::InitStatusCallback callback,
                      scoped_refptr<SharedProtoDatabase> shared_db);
  void OnGetSharedDBClient(
      std::unique_ptr<ProtoDatabase<T>> unique_db,
      bool use_shared_db,
      Callbacks::InitStatusCallback callback,
      std::unique_ptr<SharedProtoDatabaseClient<T>> client);
  void OnMigrationTransferComplete(std::unique_ptr<ProtoDatabase<T>> unique_db,
                                   std::unique_ptr<ProtoDatabase<T>> client,
                                   bool use_shared_db,
                                   Callbacks::InitStatusCallback callback,
                                   bool success);
  void OnMigrationCleanupComplete(std::unique_ptr<ProtoDatabase<T>> unique_db,
                                  std::unique_ptr<ProtoDatabase<T>> client,
                                  bool use_shared_db,
                                  Callbacks::InitStatusCallback callback,
                                  bool success);

  std::string client_namespace_;
  std::string type_prefix_;
  base::FilePath db_dir_;
  std::unique_ptr<SharedProtoDatabaseProvider> db_provider_;

  std::unique_ptr<ProtoDatabase<T>> db_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<MigrationDelegate<T>> migration_delegate_;

  std::unique_ptr<base::WeakPtrFactory<ProtoDatabaseWrapper<T>>>
      weak_ptr_factory_;
};

template <typename T>
void ProtoDatabaseWrapper<T>::RunCallbackOnCallingSequence(
    base::OnceClosure callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

template <typename T>
ProtoDatabaseWrapper<T>::ProtoDatabaseWrapper(
    const std::string& client_namespace,
    const std::string& type_prefix,
    const base::FilePath& db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<SharedProtoDatabaseProvider> db_provider)
    : UniqueProtoDatabase<T>(task_runner) {
  client_namespace_ = client_namespace;
  type_prefix_ = type_prefix;
  db_dir_ = db_dir;
  db_ = nullptr;
  db_provider_ = std::move(db_provider);
  task_runner_ = task_runner;
  migration_delegate_ = std::make_unique<MigrationDelegate<T>>();
  weak_ptr_factory_ =
      std::make_unique<base::WeakPtrFactory<ProtoDatabaseWrapper<T>>>(this);
}

template <typename T>
void ProtoDatabaseWrapper<T>::Init(const char* client_name,
                                   const base::FilePath& database_dir,
                                   const leveldb_env::Options& options,
                                   Callbacks::InitCallback callback) {
  // We shouldn't be calling this on the wrapper. This function is purely for
  // compatibility right now.
  NOTREACHED();
}

template <typename T>
void ProtoDatabaseWrapper<T>::Init(
    const std::string& client_name,
    typename Callbacks::InitStatusCallback callback) {
  bool use_shared_db =
      SharedProtoDatabaseClientList::ShouldUseSharedDB(client_name);
  Init(client_name, use_shared_db, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::Init(const std::string& client_name,
                                   bool use_shared_db,
                                   Callbacks::InitStatusCallback callback) {
  auto options = CreateSimpleOptions();
  // If we're NOT using a shared DB, we want to force creation of the unique one
  // because that's what we expect to be using moving forward. If we ARE using
  // the shared DB, we only want to see if there's a unique DB that already
  // exists and can be opened so that we can perform migration if necessary.
  options.create_if_missing = !use_shared_db;
  InitUniqueDB(client_name, options, use_shared_db, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::InitUniqueDB(
    const std::string& client_name,
    const leveldb_env::Options& options,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  auto unique_db = std::make_unique<UniqueProtoDatabase<T>>(db_dir_, options,
                                                            this->task_runner_);
  auto* unique_db_ptr = unique_db.get();
  unique_db_ptr->Init(
      client_name,
      base::BindOnce(&ProtoDatabaseWrapper<T>::OnInitUniqueDB,
                     weak_ptr_factory_->GetWeakPtr(), std::move(unique_db),
                     use_shared_db, std::move(callback)));
}

template <typename T>
void ProtoDatabaseWrapper<T>::OnInitUniqueDB(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    Enums::InitStatus status) {
  // If the unique DB is corrupt, just return it early with the corruption
  // status to avoid silently migrating a corrupt database and giving no errors
  // back.
  if (status == Enums::InitStatus::kCorrupt) {
    db_ = std::move(unique_db);
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), Enums::InitStatus::kCorrupt));
    return;
  }

  // Clear out the unique_db before sending an unusable DB into InitSharedDB,
  // a nullptr indicates opening a unique DB failed.
  if (status != Enums::InitStatus::kOK)
    unique_db.reset();

  GetSharedDBClient(std::move(unique_db), use_shared_db, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::GetSharedDBClient(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  // Get the current task runner to ensure the callback is run on the same
  // callback as the rest, and the WeakPtr checks out on the right sequence.
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();

  db_provider_->GetDBInstance(
      base::BindOnce(&ProtoDatabaseWrapper<T>::OnInitSharedDB,
                     weak_ptr_factory_->GetWeakPtr(), std::move(unique_db),
                     use_shared_db, std::move(callback)),
      current_task_runner);
}

template <typename T>
void ProtoDatabaseWrapper<T>::OnInitSharedDB(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<SharedProtoDatabase> shared_db) {
  if (shared_db) {
    // If we have a reference to the shared database, try to get a client.
    shared_db->GetClientAsync<T>(
        client_namespace_, type_prefix_, use_shared_db,
        base::BindOnce(&ProtoDatabaseWrapper<T>::OnGetSharedDBClient,
                       weak_ptr_factory_->GetWeakPtr(), std::move(unique_db),
                       use_shared_db, std::move(callback)));
    return;
  }

  // Otherwise, we just call the OnGetSharedDBClient function with a nullptr
  // client.
  OnGetSharedDBClient(std::move(unique_db), use_shared_db, std::move(callback),
                      nullptr);
}

template <typename T>
void ProtoDatabaseWrapper<T>::OnGetSharedDBClient(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    std::unique_ptr<SharedProtoDatabaseClient<T>> client) {
  if (!unique_db && !client) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), Enums::InitStatus::kError));
    return;
  }

  if (unique_db && client) {
    // We got access to both the unique DB and a shared DB, meaning we need to
    // attempt migration and give back the right one.
    ProtoDatabase<T>* from = use_shared_db ? unique_db.get() : client.get();
    ProtoDatabase<T>* to = use_shared_db ? client.get() : unique_db.get();
    migration_delegate_->DoMigration(
        from, to,
        base::BindOnce(&ProtoDatabaseWrapper<T>::OnMigrationTransferComplete,
                       weak_ptr_factory_->GetWeakPtr(), std::move(unique_db),
                       std::move(client), use_shared_db, std::move(callback)));
    return;
  }

  // One of two things happened:
  // 1) We failed to get a shared DB instance, so regardless of whether we
  // want to use the shared DB or not, we'll settle for the unique DB.
  // 2) We failed to initialize a unique DB, but got  access to the shared DB,
  // so we use that regardless of whether we "should be" or not.
  db_ = client ? std::move(client) : std::move(unique_db);
  RunCallbackOnCallingSequence(
      base::BindOnce(std::move(callback), Enums::InitStatus::kOK));
}

template <typename T>
void ProtoDatabaseWrapper<T>::OnMigrationTransferComplete(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    std::unique_ptr<ProtoDatabase<T>> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  if (success) {
    // Call Destroy on the DB we no longer want to use.
    // TODO(thildebr): If this destroy fails, we should flag this as undestroyed
    // so that we don't erroneously transfer data from the undestroyed database
    // on next start. This might be possible by comparing the modified timestamp
    // of the unique database directory with metadata in the shared database
    // about last modified for each client.
    auto* db_destroy_ptr = use_shared_db ? unique_db.get() : client.get();
    db_destroy_ptr->Destroy(
        base::BindOnce(&ProtoDatabaseWrapper<T>::OnMigrationCleanupComplete,
                       weak_ptr_factory_->GetWeakPtr(), std::move(unique_db),
                       std::move(client), use_shared_db, std::move(callback)));
    return;
  }

  db_ = use_shared_db ? std::move(unique_db) : std::move(client);
  RunCallbackOnCallingSequence(
      base::BindOnce(std::move(callback), Enums::InitStatus::kOK));
}

template <typename T>
void ProtoDatabaseWrapper<T>::OnMigrationCleanupComplete(
    std::unique_ptr<ProtoDatabase<T>> unique_db,
    std::unique_ptr<ProtoDatabase<T>> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  // We still return true in our callback below because we do have a database as
  // far as the original caller is concerned. As long as |db_| is assigned, we
  // return true.
  if (success) {
    db_ = use_shared_db ? std::move(client) : std::move(unique_db);
  } else {
    db_ = use_shared_db ? std::move(unique_db) : std::move(client);
  }
  RunCallbackOnCallingSequence(
      base::BindOnce(std::move(callback), Enums::InitStatus::kOK));
}

template <typename T>
void ProtoDatabaseWrapper<T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    Callbacks::InitStatusCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), Enums::InitStatus::kError));
    return;
  }

  db_->InitWithDatabase(database, database_dir, options, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(std::move(callback), false));
    return;
  }

  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(std::move(callback), false));
    return;
  }

  db_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                     delete_key_filter, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(std::move(callback), false));
    return;
  }

  db_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                     delete_key_filter, target_prefix,
                                     std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(
        std::move(callback), false, std::make_unique<std::vector<T>>()));
    return;
  }

  db_->LoadEntries(std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(
        std::move(callback), false, std::make_unique<std::vector<T>>()));
    return;
  }

  db_->LoadEntriesWithFilter(filter, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(
        std::move(callback), false, std::make_unique<std::vector<T>>()));
    return;
  }

  db_->LoadEntriesWithFilter(key_filter, options, target_prefix,
                             std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false,
                       std::make_unique<std::map<std::string, T>>()));
    return;
  }

  db_->LoadKeysAndEntries(std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false,
                       std::make_unique<std::map<std::string, T>>()));
    return;
  }

  db_->LoadKeysAndEntriesWithFilter(filter, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false,
                       std::make_unique<std::map<std::string, T>>()));
    return;
  }

  db_->LoadKeysAndEntriesWithFilter(filter, options, target_prefix,
                                    std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadKeys(Callbacks::LoadKeysCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false,
                       std::make_unique<std::vector<std::string>>()));
    return;
  }

  db_->LoadKeys(std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::LoadKeys(const std::string& target_prefix,
                                       Callbacks::LoadKeysCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false,
                       std::make_unique<std::vector<std::string>>()));
    return;
  }

  db_->LoadKeys(target_prefix, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(
        base::BindOnce(std::move(callback), false, std::make_unique<T>()));
    return;
  }

  db_->GetEntry(key, std::move(callback));
}

template <typename T>
void ProtoDatabaseWrapper<T>::Destroy(Callbacks::DestroyCallback callback) {
  if (!db_) {
    RunCallbackOnCallingSequence(base::BindOnce(std::move(callback), false));
    return;
  }

  db_->Destroy(std::move(callback));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_WRAPPER_H_