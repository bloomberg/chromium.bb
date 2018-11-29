// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_

#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

// An implementation of ProtoDatabase<T> that manages the lifecycle of a unique
// LevelDB instance.
template <typename T>
class UniqueProtoDatabase : public ProtoDatabase<T> {
 public:
  explicit UniqueProtoDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  explicit UniqueProtoDatabase(std::unique_ptr<ProtoLevelDBWrapper>);
  virtual ~UniqueProtoDatabase();

  UniqueProtoDatabase(
      const base::FilePath& database_dir,
      const leveldb_env::Options& options,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // This version of Init is for compatibility, since many of the current
  // proto database still use this.
  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const leveldb_env::Options& options,
            typename ProtoDatabase<T>::InitCallback callback) override;

  void Init(const std::string& client_name,
            typename ProtoDatabase<T>::InitCallback callback) override;

  void InitWithDatabase(
      LevelDB* database,
      const base::FilePath& database_dir,
      const leveldb_env::Options& options,
      typename ProtoDatabase<T>::InitCallback callback) override;

  void UpdateEntries(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      typename ProtoDatabase<T>::UpdateCallback callback) override;

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      typename ProtoDatabase<T>::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      typename ProtoDatabase<T>::UpdateCallback callback) override;

  void LoadEntries(typename ProtoDatabase<T>::LoadCallback callback) override;

  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename ProtoDatabase<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename ProtoDatabase<T>::LoadCallback callback) override;

  void LoadKeysAndEntries(
      typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) override;

  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) override;
  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) override;

  void LoadKeys(typename ProtoDatabase<T>::LoadKeysCallback callback) override;
  void LoadKeys(const std::string& target_prefix,
                typename ProtoDatabase<T>::LoadKeysCallback callback) override;

  void GetEntry(const std::string& key,
                typename ProtoDatabase<T>::GetCallback callback) override;

  void Destroy(typename ProtoDatabase<T>::DestroyCallback callback) override;

  bool GetApproximateMemoryUse(uint64_t* approx_mem_use);

  // Sets the identifier used by the underlying LevelDB wrapper to record
  // metrics.
  void SetMetricsId(const std::string& id);

 protected:
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  THREAD_CHECKER(thread_checker_);

  base::FilePath database_dir_;
  leveldb_env::Options options_;
  std::unique_ptr<LevelDB> db_;
};

template <typename T>
UniqueProtoDatabase<T>::UniqueProtoDatabase(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner)) {}

template <typename T>
UniqueProtoDatabase<T>::UniqueProtoDatabase(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper)
    : db_wrapper_(std::move(db_wrapper)) {}

template <typename T>
UniqueProtoDatabase<T>::UniqueProtoDatabase(
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : UniqueProtoDatabase<T>(task_runner) {
  database_dir_ = database_dir;
  options_ = options;
}

template <typename T>
UniqueProtoDatabase<T>::~UniqueProtoDatabase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (db_.get() &&
      !db_wrapper_->task_runner()->DeleteSoon(FROM_HERE, db_.release()))
    DLOG(WARNING) << "Proto database will not be deleted.";
}

template <typename T>
void UniqueProtoDatabase<T>::Init(
    const std::string& client_name,
    typename ProtoDatabase<T>::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  db_ = std::make_unique<LevelDB>(client_name.c_str());
  db_wrapper_->SetMetricsId(client_name);
  InitWithDatabase(db_.get(), database_dir_, options_, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::Init(
    const char* client_name,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    typename ProtoDatabase<T>::InitCallback callback) {
  database_dir_ = database_dir;
  options_ = options;
  db_wrapper_->SetMetricsId(client_name);
  Init(std::string(client_name), std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    typename ProtoDatabase<T>::InitCallback callback) {
  // We set |destroy_on_corruption| to true to preserve the original behaviour
  // where database corruption led to automatic destruction.
  db_wrapper_->InitWithDatabase(database, database_dir, options,
                                true /* destroy_on_corruption */,
                                std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  db_wrapper_->template UpdateEntries<T>(std::move(entries_to_save),
                                         std::move(keys_to_remove),
                                         std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  db_wrapper_->template UpdateEntriesWithRemoveFilter<T>(
      std::move(entries_to_save), delete_key_filter, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  db_wrapper_->template UpdateEntriesWithRemoveFilter<T>(
      std::move(entries_to_save), delete_key_filter, target_prefix,
      std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntries(
    typename ProtoDatabase<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntries<T>(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename ProtoDatabase<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntriesWithFilter<T>(filter, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename ProtoDatabase<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntriesWithFilter<T>(
      key_filter, options, target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntries(
    typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntries<T>(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntriesWithFilter<T>(filter,
                                                        std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename ProtoDatabase<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntriesWithFilter<T>(
      filter, options, target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeys(
    typename ProtoDatabase<T>::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeys(
    const std::string& target_prefix,
    typename ProtoDatabase<T>::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::GetEntry(
    const std::string& key,
    typename ProtoDatabase<T>::GetCallback callback) {
  db_wrapper_->template GetEntry<T>(key, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::Destroy(
    typename ProtoDatabase<T>::DestroyCallback callback) {
  db_wrapper_->Destroy(std::move(callback));
}

template <typename T>
bool UniqueProtoDatabase<T>::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  return db_wrapper_->GetApproximateMemoryUse(approx_mem_use);
}

template <typename T>
void UniqueProtoDatabase<T>::SetMetricsId(const std::string& id) {
  db_wrapper_->SetMetricsId(id);
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
