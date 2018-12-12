// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_WRAPPER_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_WRAPPER_H_

#include "base/files/file_path.h"

#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/unique_proto_database.h"

namespace leveldb_proto {

// The ProtoDatabaseWrapper<T> owns a ProtoDatabase<T> instance, and allows the
// underlying ProtoDatabase<T> implementation to change without users of the
// wrapper needing to know.
// This allows clients to request a DB instance without knowing whether or not
// it's a UniqueDatabase<T> or a SharedProtoDatabaseClient<T>.
template <typename T>
class ProtoDatabaseWrapper : public UniqueProtoDatabase<T> {
 public:
  ProtoDatabaseWrapper(
      const std::string& client_namespace,
      const std::string& type_prefix,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  virtual ~ProtoDatabaseWrapper() {}

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
  void OnInitUniqueDB(std::unique_ptr<ProtoDatabase<T>> db,
                      Callbacks::InitStatusCallback callback,
                      Enums::InitStatus status);

  std::string client_namespace_;
  std::string type_prefix_;
  base::FilePath db_dir_;
  std::unique_ptr<ProtoDatabase<T>> db_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

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
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : UniqueProtoDatabase<T>(task_runner) {
  client_namespace_ = client_namespace;
  type_prefix_ = type_prefix;
  db_dir_ = db_dir;
  db_ = nullptr;
  task_runner_ = task_runner;
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
void ProtoDatabaseWrapper<T>::OnInitUniqueDB(
    std::unique_ptr<ProtoDatabase<T>> db,
    Callbacks::InitStatusCallback callback,
    Enums::InitStatus status) {
  // The DB is still technically usable after corruption, so treat that as
  // success.
  bool success =
      status == Enums::InitStatus::kOK || status == Enums::InitStatus::kCorrupt;
  if (success)
    db_ = std::move(db);
  RunCallbackOnCallingSequence(base::BindOnce(std::move(callback), status));
}

template <typename T>
void ProtoDatabaseWrapper<T>::Init(const std::string& client_name,
                                   Callbacks::InitStatusCallback callback) {
  auto unique_db = std::make_unique<UniqueProtoDatabase<T>>(
      db_dir_, CreateSimpleOptions(), this->task_runner_);
  unique_db->Init(client_name,
                  base::BindOnce(&ProtoDatabaseWrapper<T>::OnInitUniqueDB,
                                 weak_ptr_factory_->GetWeakPtr(),
                                 std::move(unique_db), std::move(callback)));
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