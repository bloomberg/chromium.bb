// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}

namespace leveldb_proto {

using KeyValueVector = base::StringPairs;
using KeyVector = std::vector<std::string>;

// When the ProtoDatabase instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
// Construction/calls/destruction should all happen on the same thread.
class ProtoLevelDBWrapper {
 public:
  // All blocking calls/disk access will happen on the provided |task_runner|.
  ProtoLevelDBWrapper(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ProtoLevelDBWrapper(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      LevelDB* db);

  virtual ~ProtoLevelDBWrapper();

  template <typename T>
  void UpdateEntries(std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
                         entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback);

  template <typename T>
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback);

  template <typename T>
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename Util::Internal<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback);

  template <typename T>
  void LoadEntries(typename Callbacks::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadEntriesWithFilter(
      const KeyFilter& key_filter,
      typename Callbacks::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadEntriesWithFilter(
      const KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback);

  template <typename T>
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback);

  template <typename T>
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback);

  template <typename T>
  void LoadKeysAndEntriesWhile(
      const KeyFilter& while_callback,
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback);

  template <typename T>
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback);

  void LoadKeys(Callbacks::LoadKeysCallback callback);
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback);

  template <typename T>
  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback);

  void RemoveKeys(const KeyFilter& filter,
                  const std::string& target_prefix,
                  Callbacks::UpdateCallback callback);

  void Destroy(Callbacks::DestroyCallback callback);

  void RunInitCallback(Callbacks::InitCallback callback,
                       const leveldb::Status* status);

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        bool destroy_on_corruption,
                        Callbacks::InitStatusCallback callback);

  void SetMetricsId(const std::string& id);

  bool GetApproximateMemoryUse(uint64_t* approx_mem_use);

  const scoped_refptr<base::SequencedTaskRunner>& task_runner();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to run blocking tasks in-order, must be the TaskRunner that |db_|
  // relies on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  LevelDB* db_ = nullptr;

  // The identifier used when recording metrics to determine the source of the
  // LevelDB calls, likely the database client name.
  std::string metrics_id_ = "Default";

  base::WeakPtrFactory<ProtoLevelDBWrapper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProtoLevelDBWrapper);
};

namespace {

template <typename T>
void RunLoadCallback(typename Callbacks::Internal<T>::LoadCallback callback,
                     bool* success,
                     std::unique_ptr<std::vector<T>> entries) {
  std::move(callback).Run(*success, std::move(entries));
}

template <typename T>
void RunLoadKeysAndEntriesCallback(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback,
    bool* success,
    std::unique_ptr<std::map<std::string, T>> keys_entries) {
  std::move(callback).Run(*success, std::move(keys_entries));
}

template <typename T>
void RunGetCallback(typename Callbacks::Internal<T>::GetCallback callback,
                    const bool* success,
                    const bool* found,
                    std::unique_ptr<T> entry) {
  std::move(callback).Run(*success, *found ? std::move(entry) : nullptr);
}

template <typename T>
bool UpdateEntriesFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    const std::string& client_id) {
  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (const auto& pair : *entries_to_save) {
    pairs_to_save.push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  leveldb::Status status;
  bool success = database->Save(pairs_to_save, *keys_to_remove, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  return success;
}

template <typename T>
bool UpdateEntriesWithRemoveFilterFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    const std::string& client_id) {
  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (const auto& pair : *entries_to_save) {
    pairs_to_save.push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  leveldb::Status status;
  bool success = database->UpdateWithRemoveFilter(
      pairs_to_save, delete_key_filter, target_prefix, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  return success;
}

template <typename T>
void LoadKeysAndEntriesFromTaskRunner(LevelDB* database,
                                      const KeyFilter& while_callback,
                                      const KeyFilter& filter,
                                      const leveldb::ReadOptions& options,
                                      const std::string& target_prefix,
                                      const std::string& client_id,
                                      bool* success,
                                      std::map<std::string, T>* keys_entries) {
  DCHECK(success);
  DCHECK(keys_entries);
  keys_entries->clear();

  std::map<std::string, std::string> loaded_entries;
  *success = database->LoadKeysAndEntriesWhile(filter, &loaded_entries, options,
                                               target_prefix, while_callback);

  for (const auto& pair : loaded_entries) {
    T entry;
    if (!entry.ParseFromString(pair.second)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry";
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }

    keys_entries->insert(std::make_pair(pair.first, entry));
  }

  ProtoLevelDBWrapperMetrics::RecordLoadKeysAndEntries(client_id, success);
}

template <typename T>
void LoadEntriesFromTaskRunner(LevelDB* database,
                               const KeyFilter& filter,
                               const leveldb::ReadOptions& options,
                               const std::string& target_prefix,
                               const std::string& client_id,
                               bool* success,
                               std::vector<T>* entries) {
  std::vector<std::string> loaded_entries;
  *success =
      database->LoadWithFilter(filter, &loaded_entries, options, target_prefix);

  for (const auto& serialized_entry : loaded_entries) {
    T entry;
    if (!entry.ParseFromString(serialized_entry)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry";
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }

    entries->push_back(entry);
  }

  ProtoLevelDBWrapperMetrics::RecordLoadEntries(client_id, success);
}

template <typename T>
void GetEntryFromTaskRunner(LevelDB* database,
                            const std::string& key,
                            const std::string& client_id,
                            bool* success,
                            bool* found,
                            T* entry) {
  std::string serialized_entry;
  leveldb::Status status;
  *success = database->Get(key, found, &serialized_entry, &status);

  if (*found && !entry->ParseFromString(serialized_entry)) {
    *found = false;
    DLOG(WARNING) << "Unable to parse leveldb_proto entry";
  }

  ProtoLevelDBWrapperMetrics::RecordGet(client_id, *success, *found, status);
}

}  // namespace

template <typename T>
void ProtoLevelDBWrapper::UpdateEntries(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    typename Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(UpdateEntriesFromTaskRunner<T>, base::Unretained(db_),
                     std::move(entries_to_save), std::move(keys_to_remove),
                     metrics_id_),
      std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter<T>(std::move(entries_to_save),
                                   delete_key_filter, std::string(),
                                   std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(UpdateEntriesWithRemoveFilterFromTaskRunner<T>,
                     base::Unretained(db_), std::move(entries_to_save),
                     delete_key_filter, target_prefix, metrics_id_),
      std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter<T>(KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter<T>(key_filter, leveldb::ReadOptions(), std::string(),
                           std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  auto entries = std::make_unique<std::vector<T>>();
  // Get this pointer before |entries| is std::move()'d so we can use it below.
  auto* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadEntriesFromTaskRunner<T>, base::Unretained(db_),
                     key_filter, options, target_prefix, metrics_id_, success,
                     entries_ptr),
      base::BindOnce(RunLoadCallback<T>, std::move(callback),
                     base::Owned(success), std::move(entries)));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter<T>(KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const KeyFilter& key_filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter<T>(key_filter, leveldb::ReadOptions(),
                                  std::string(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWhile<T>(
      base::BindRepeating(
          [](const std::string& prefix, const std::string& key) {
            return base::StartsWith(key, prefix, base::CompareCase::SENSITIVE);
          },
          target_prefix),
      key_filter, options, target_prefix, std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWhile<T>(
      base::BindRepeating(
          [](const std::string& range_end, const std::string& key) {
            return key.compare(range_end) <= 0;
          },
          end),
      KeyFilter(), leveldb::ReadOptions(), start, std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesWhile(
    const KeyFilter& while_callback,
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  auto keys_entries = std::make_unique<std::map<std::string, T>>();
  // Get this pointer before |keys_entries| is std::move()'d so we can use it
  // below.
  auto* keys_entries_ptr = keys_entries.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadKeysAndEntriesFromTaskRunner<T>, base::Unretained(db_),
                     while_callback, filter, options, target_prefix,
                     metrics_id_, success, keys_entries_ptr),
      base::BindOnce(RunLoadKeysAndEntriesCallback<T>, std::move(callback),
                     base::Owned(success), std::move(keys_entries)));
}

template <typename T>
void ProtoLevelDBWrapper::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  bool* found = new bool(false);
  auto entry = std::make_unique<T>();
  // Get this pointer before |entry| is std::move()'d so we can use it below.
  auto* entry_ptr = entry.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(GetEntryFromTaskRunner<T>, base::Unretained(db_), key,
                     metrics_id_, success, found, entry_ptr),
      base::BindOnce(RunGetCallback<T>, std::move(callback),
                     base::Owned(success), base::Owned(found),
                     std::move(entry)));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_
