// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
#define COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {

// A temporary wrapper around the Storage Service's DomStorageDatabase class,
// consumed by LocalStorageContextMojo, SessionStorageContextMojo, and related
// classes.
//
// TODO(https://crbug.com/1000959): Delete this class.
class LevelDBDatabaseImpl {
 public:
  using StatusCallback = base::OnceCallback<void(Status)>;

  ~LevelDBDatabaseImpl();

  static std::unique_ptr<LevelDBDatabaseImpl> OpenDirectory(
      const leveldb_env::Options& options,
      const base::FilePath& directory,
      const std::string& dbname,
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  static std::unique_ptr<LevelDBDatabaseImpl> OpenInMemory(
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const std::string& tracking_name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  base::SequenceBound<storage::DomStorageDatabase>& database() {
    return database_;
  }
  const base::SequenceBound<storage::DomStorageDatabase>& database() const {
    return database_;
  }

  // Overridden from LevelDBDatabase:
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           StatusCallback callback);

  void Delete(const std::vector<uint8_t>& key, StatusCallback callback);

  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      StatusCallback callback);

  void RewriteDB(StatusCallback callback);

  void Write(std::vector<mojom::BatchedOperationPtr> operations,
             StatusCallback callback);

  using GetCallback =
      base::OnceCallback<void(Status status, const std::vector<uint8_t>&)>;
  void Get(const std::vector<uint8_t>& key, GetCallback callback);

  using GetPrefixedCallback =
      base::OnceCallback<void(Status status, std::vector<mojom::KeyValuePtr>)>;
  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   GetPrefixedCallback callback);

  void CopyPrefixed(const std::vector<uint8_t>& source_key_prefix,
                    const std::vector<uint8_t>& destination_key_prefix,
                    StatusCallback callback);

  template <typename ResultType>
  void RunDatabaseTask(
      base::OnceCallback<ResultType(const storage::DomStorageDatabase&)> task,
      base::OnceCallback<void(ResultType)> callback) {
    auto wrapped_task = base::BindOnce(
        [](base::OnceCallback<ResultType(const storage::DomStorageDatabase&)>
               task,
           base::OnceCallback<void(ResultType)> callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           const storage::DomStorageDatabase& db) {
          callback_task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(task).Run(db)));
        },
        std::move(task), std::move(callback),
        base::SequencedTaskRunnerHandle::Get());
    if (database_) {
      database_.PostTaskWithThisObject(FROM_HERE, std::move(wrapped_task));
    } else {
      tasks_to_run_on_open_.push_back(std::move(wrapped_task));
    }
  }

 private:
  using StatusAndKeyValues =
      std::tuple<Status, std::vector<mojom::KeyValuePtr>>;

  void OnDatabaseOpened(
      StatusCallback callback,
      base::SequenceBound<storage::DomStorageDatabase> database,
      leveldb::Status status);

  explicit LevelDBDatabaseImpl();

  base::SequenceBound<storage::DomStorageDatabase> database_;

  using DatabaseTask =
      base::OnceCallback<void(const storage::DomStorageDatabase&)>;
  std::vector<DatabaseTask> tasks_to_run_on_open_;

  base::WeakPtrFactory<LevelDBDatabaseImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LevelDBDatabaseImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_LEVELDB_LEVELDB_DATABASE_IMPL_H_
