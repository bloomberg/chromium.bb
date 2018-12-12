// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_H_
#define COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_H_

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/unique_proto_database.h"

namespace leveldb_proto {

class SharedProtoDatabase;

std::string StripPrefix(const std::string& key, const std::string& prefix);
std::unique_ptr<std::vector<std::string>> PrefixStrings(
    std::unique_ptr<std::vector<std::string>> strings,
    const std::string& prefix);
bool KeyFilterStripPrefix(const LevelDB::KeyFilter& key_filter,
                          const std::string& prefix,
                          const std::string& key);
std::unique_ptr<std::vector<std::string>> PrefixStrings(
    std::unique_ptr<std::vector<std::string>> strings,
    const std::string& prefix);

void GetSharedDatabaseInitStatusAsync(
    const scoped_refptr<SharedProtoDatabase>& db,
    Callbacks::InitStatusCallback callback);

// An implementation of ProtoDatabase<T> that uses a shared LevelDB and task
// runner.
// Should be created, destroyed, and used on the same thread.
template <typename T>
class SharedProtoDatabaseClient : public ProtoDatabase<T> {
 public:
  virtual ~SharedProtoDatabaseClient();

  void Init(const std::string& client_name,
            Callbacks::InitStatusCallback callback) override;

  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const leveldb_env::Options& options,
            Callbacks::InitCallback callback) override;
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        Callbacks::InitStatusCallback callback) override;

  // Overrides for prepending namespace and type prefix to all operations on the
  // shared database.
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

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback) override;

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

  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  typename Callbacks::InitCallback GetInitCallback() const;

  void SetIsCorrupt(bool is_corrupt);

 private:
  friend class SharedProtoDatabase;
  friend class SharedProtoDatabaseTest;
  friend class SharedProtoDatabaseClientTest;

  // Hide this so clients can only be created by the SharedProtoDatabase.
  SharedProtoDatabaseClient(
      std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
      const std::string& client_namespace,
      const std::string& type_prefix,
      const scoped_refptr<SharedProtoDatabase>& parent_db);

  void OnParentInit(bool success);

  static void StripPrefixLoadKeysCallback(
      Callbacks::LoadKeysCallback callback,
      const std::string& prefix,
      bool success,
      std::unique_ptr<leveldb_proto::KeyVector> keys);
  static void StripPrefixLoadKeysAndEntriesCallback(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback,
      const std::string& prefix,
      bool success,
      std::unique_ptr<std::map<std::string, T>> keys_entries);

  static std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
  PrefixKeyEntryVector(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> kev,
      const std::string& prefix);

  SEQUENCE_CHECKER(sequence_checker_);

  // |is_corrupt_| should be set by the SharedProtoDatabase that creates this
  // when a client is created that doesn't know about a previous shared
  // database corruption.
  bool is_corrupt_ = false;
  std::string prefix_;

  scoped_refptr<SharedProtoDatabase> parent_db_;
  std::unique_ptr<UniqueProtoDatabase<T>> unique_db_;

  base::WeakPtrFactory<SharedProtoDatabaseClient<T>> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabaseClient);
};

template <typename T>
SharedProtoDatabaseClient<T>::SharedProtoDatabaseClient(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
    const std::string& client_namespace,
    const std::string& type_prefix,
    const scoped_refptr<SharedProtoDatabase>& parent_db)
    : prefix_(base::JoinString({client_namespace, type_prefix, std::string()},
                               "_")),
      parent_db_(parent_db),
      unique_db_(
          std::make_unique<UniqueProtoDatabase<T>>(std::move(db_wrapper))),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename T>
SharedProtoDatabaseClient<T>::~SharedProtoDatabaseClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

template <typename T>
void SharedProtoDatabaseClient<T>::Init(
    const std::string& client_name,
    Callbacks::InitStatusCallback callback) {
  unique_db_->SetMetricsId(client_name);
  GetSharedDatabaseInitStatusAsync(parent_db_, std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::Init(const char* client_name,
                                        const base::FilePath& database_dir,
                                        const leveldb_env::Options& options,
                                        Callbacks::InitCallback callback) {
  NOTREACHED();
}

template <typename T>
void SharedProtoDatabaseClient<T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    Callbacks::InitStatusCallback callback) {
  NOTREACHED();
}

template <typename T>
void SharedProtoDatabaseClient<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->UpdateEntries(
      PrefixKeyEntryVector(std::move(entries_to_save), prefix_),
      PrefixStrings(std::move(keys_to_remove), prefix_), std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter(std::move(entries_to_save), delete_key_filter,
                                std::string(), std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->UpdateEntriesWithRemoveFilter(
      PrefixKeyEntryVector(std::move(entries_to_save), prefix_),
      base::BindRepeating(&KeyFilterStripPrefix, delete_key_filter, prefix_),
      prefix_ + target_prefix, std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadEntriesWithFilter(LevelDB::KeyFilter(), std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                        std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->LoadEntriesWithFilter(
      base::BindRepeating(&KeyFilterStripPrefix, filter, prefix_), options,
      prefix_ + target_prefix, std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadKeys(
    Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadKeys(std::string(), std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadKeys(
    const std::string& target_prefix,
    Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->LoadKeys(
      prefix_ + target_prefix,
      base::BindOnce(&SharedProtoDatabaseClient<T>::StripPrefixLoadKeysCallback,
                     std::move(callback), prefix_));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(LevelDB::KeyFilter(), std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                               std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->LoadKeysAndEntriesWithFilter(
      filter, options, prefix_ + target_prefix,
      base::BindOnce(
          &SharedProtoDatabaseClient<T>::StripPrefixLoadKeysAndEntriesCallback,
          std::move(callback), prefix_));
}

template <typename T>
void SharedProtoDatabaseClient<T>::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unique_db_->GetEntry(prefix_ + key, std::move(callback));
}

template <typename T>
void SharedProtoDatabaseClient<T>::Destroy(
    Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter(
      std::make_unique<typename Util::Internal<T>::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindOnce([](Callbacks::DestroyCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

// static
template <typename T>
void SharedProtoDatabaseClient<T>::StripPrefixLoadKeysCallback(
    Callbacks::LoadKeysCallback callback,
    const std::string& prefix,
    bool success,
    std::unique_ptr<leveldb_proto::KeyVector> keys) {
  auto stripped_keys = std::make_unique<leveldb_proto::KeyVector>();
  for (auto& key : *keys)
    stripped_keys->emplace_back(StripPrefix(key, prefix));
  std::move(callback).Run(success, std::move(stripped_keys));
}

// static
template <typename T>
void SharedProtoDatabaseClient<T>::StripPrefixLoadKeysAndEntriesCallback(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback,
    const std::string& prefix,
    bool success,
    std::unique_ptr<std::map<std::string, T>> keys_entries) {
  auto stripped_keys_map = std::make_unique<std::map<std::string, T>>();
  for (auto& key_entry : *keys_entries) {
    stripped_keys_map->insert(
        std::make_pair(StripPrefix(key_entry.first, prefix), key_entry.second));
  }
  std::move(callback).Run(success, std::move(stripped_keys_map));
}

// static
template <typename T>
std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
SharedProtoDatabaseClient<T>::PrefixKeyEntryVector(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> kev,
    const std::string& prefix) {
  for (auto& key_entry_pair : *kev) {
    key_entry_pair.first = base::StrCat({prefix, key_entry_pair.first});
  }
  return kev;
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_H_
