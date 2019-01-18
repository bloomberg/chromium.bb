// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_

#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/public/proto_database.h"

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
            typename Callbacks::InitCallback callback) override;

  void Init(const std::string& client_name,
            Callbacks::InitStatusCallback callback) override;

  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        Callbacks::InitStatusCallback callback);

  void UpdateEntries(std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
                         entries_to_save,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     Callbacks::UpdateCallback callback) override;

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback) override;

  void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback) override;

  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  void RemoveKeysForTesting(const KeyFilter& key_filter,
                            const std::string& target_prefix,
                            Callbacks::UpdateCallback callback);

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
      !db_wrapper_->task_runner()->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "Proto database will not be deleted.";
  }
}

template <typename T>
void UniqueProtoDatabase<T>::Init(const std::string& client_name,
                                  Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  db_ = std::make_unique<LevelDB>(client_name.c_str());
  db_wrapper_->SetMetricsId(client_name);
  InitWithDatabase(db_.get(), database_dir_, options_, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::Init(const char* client_name,
                                  const base::FilePath& database_dir,
                                  const leveldb_env::Options& options,
                                  Callbacks::InitCallback callback) {
  database_dir_ = database_dir;
  options_ = options;
  db_wrapper_->SetMetricsId(client_name);
  Init(std::string(client_name),
       base::BindOnce(
           [](Callbacks::InitCallback callback, Enums::InitStatus status) {
             std::move(callback).Run(status == Enums::InitStatus::kOK);
           },
           std::move(callback)));
}

template <typename T>
void UniqueProtoDatabase<T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    Callbacks::InitStatusCallback callback) {
  // We set |destroy_on_corruption| to true to preserve the original behaviour
  // where database corruption led to automatic destruction.
  db_wrapper_->InitWithDatabase(database, database_dir, options,
                                true /* destroy_on_corruption */,
                                std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntries(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->template UpdateEntries<T>(std::move(entries_to_save),
                                         std::move(keys_to_remove),
                                         std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->template UpdateEntriesWithRemoveFilter<T>(
      std::move(entries_to_save), delete_key_filter, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->template UpdateEntriesWithRemoveFilter<T>(
      std::move(entries_to_save), delete_key_filter, target_prefix,
      std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntries<T>(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntriesWithFilter<T>(filter, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  db_wrapper_->template LoadEntriesWithFilter<T>(
      key_filter, options, target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntries<T>(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntriesWithFilter<T>(filter,
                                                        std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntriesWithFilter<T>(
      filter, options, target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->template LoadKeysAndEntriesInRange<T>(start, end,
                                                     std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeys(Callbacks::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::LoadKeys(const std::string& target_prefix,
                                      Callbacks::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(target_prefix, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  db_wrapper_->template GetEntry<T>(key, std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::Destroy(Callbacks::DestroyCallback callback) {
  db_wrapper_->Destroy(std::move(callback));
}

template <typename T>
void UniqueProtoDatabase<T>::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->RemoveKeys(key_filter, target_prefix, std::move(callback));
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

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_
