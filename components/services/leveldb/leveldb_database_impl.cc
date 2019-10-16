// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/leveldb/leveldb_database_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb {

// static
std::unique_ptr<LevelDBDatabaseImpl> LevelDBDatabaseImpl::OpenDirectory(
    const leveldb_env::Options& options,
    const base::FilePath& directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  std::unique_ptr<LevelDBDatabaseImpl> db(new LevelDBDatabaseImpl);
  storage::DomStorageDatabase::OpenDirectory(
      directory, dbname, options, memory_dump_id,
      std::move(blocking_task_runner),
      base::BindOnce(&LevelDBDatabaseImpl::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

// static
std::unique_ptr<LevelDBDatabaseImpl> LevelDBDatabaseImpl::OpenInMemory(
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const std::string& tracking_name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  std::unique_ptr<LevelDBDatabaseImpl> db(new LevelDBDatabaseImpl);
  storage::DomStorageDatabase::OpenInMemory(
      tracking_name, memory_dump_id, std::move(blocking_task_runner),
      base::BindOnce(&LevelDBDatabaseImpl::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

LevelDBDatabaseImpl::LevelDBDatabaseImpl() = default;

LevelDBDatabaseImpl::~LevelDBDatabaseImpl() = default;

void LevelDBDatabaseImpl::Put(const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& value,
                              StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](const std::vector<uint8_t>& key, const std::vector<uint8_t>& value,
             const storage::DomStorageDatabase& db) {
            return db.Put(key, value);
          },
          key, value),
      std::move(callback));
}

void LevelDBDatabaseImpl::Delete(const std::vector<uint8_t>& key,
                                 StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](const std::vector<uint8_t>& key,
             const storage::DomStorageDatabase& db) { return db.Delete(key); },
          key),
      std::move(callback));
}

void LevelDBDatabaseImpl::DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                                         StatusCallback callback) {
  RunDatabaseTask(base::BindOnce(
                      [](const std::vector<uint8_t>& prefix,
                         const storage::DomStorageDatabase& db) {
                        WriteBatch batch;
                        Status status = db.DeletePrefixed(prefix, &batch);
                        if (!status.ok())
                          return status;
                        return db.Commit(&batch);
                      },
                      key_prefix),
                  std::move(callback));
}

void LevelDBDatabaseImpl::RewriteDB(StatusCallback callback) {
  DCHECK(database_);
  database_.PostTaskWithThisObject(
      FROM_HERE,
      base::BindOnce(
          [](StatusCallback callback,
             scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             storage::DomStorageDatabase* db) {
            callback_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), db->RewriteDB()));
          },
          std::move(callback), base::SequencedTaskRunnerHandle::Get()));
}

void LevelDBDatabaseImpl::Write(
    std::vector<mojom::BatchedOperationPtr> operations,
    StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](std::vector<mojom::BatchedOperationPtr> operations,
             const storage::DomStorageDatabase& db) {
            WriteBatch batch;
            for (auto& op : operations) {
              switch (op->type) {
                case mojom::BatchOperationType::PUT_KEY: {
                  if (op->value) {
                    batch.Put(GetSliceFor(op->key), GetSliceFor(*(op->value)));
                  } else {
                    batch.Put(GetSliceFor(op->key), Slice());
                  }
                  break;
                }
                case mojom::BatchOperationType::DELETE_KEY: {
                  batch.Delete(GetSliceFor(op->key));
                  break;
                }
                case mojom::BatchOperationType::DELETE_PREFIXED_KEY: {
                  db.DeletePrefixed(op->key, &batch);
                  break;
                }
                case mojom::BatchOperationType::COPY_PREFIXED_KEY: {
                  // DCHECK is fine here since browser code is the only caller.
                  DCHECK(op->value);
                  db.CopyPrefixed(op->key, *(op->value), &batch);
                  break;
                }
              }
            }
            return db.Commit(&batch);
          },
          std::move(operations)),
      std::move(callback));
}

void LevelDBDatabaseImpl::Get(const std::vector<uint8_t>& key,
                              GetCallback callback) {
  struct GetResult {
    Status status;
    storage::DomStorageDatabase::Value value;
  };
  RunDatabaseTask(base::BindOnce(
                      [](const std::vector<uint8_t>& key,
                         const storage::DomStorageDatabase& db) {
                        GetResult result;
                        result.status = db.Get(key, &result.value);
                        return result;
                      },
                      key),
                  base::BindOnce(
                      [](GetCallback callback, GetResult result) {
                        std::move(callback).Run(result.status, result.value);
                      },
                      std::move(callback)));
}

void LevelDBDatabaseImpl::GetPrefixed(const std::vector<uint8_t>& key_prefix,
                                      GetPrefixedCallback callback) {
  struct GetPrefixedResult {
    Status status;
    std::vector<storage::DomStorageDatabase::KeyValuePair> entries;
  };

  RunDatabaseTask(
      base::BindOnce(
          [](const std::vector<uint8_t>& prefix,
             const storage::DomStorageDatabase& db) {
            GetPrefixedResult result;
            result.status = db.GetPrefixed(prefix, &result.entries);
            return result;
          },
          key_prefix),
      base::BindOnce(
          [](GetPrefixedCallback callback, GetPrefixedResult result) {
            std::vector<mojom::KeyValuePtr> entries;
            for (auto& entry : result.entries)
              entries.push_back(mojom::KeyValue::New(entry.key, entry.value));
            std::move(callback).Run(result.status, std::move(entries));
          },
          std::move(callback)));
}

void LevelDBDatabaseImpl::CopyPrefixed(
    const std::vector<uint8_t>& source_key_prefix,
    const std::vector<uint8_t>& destination_key_prefix,
    StatusCallback callback) {
  RunDatabaseTask(base::BindOnce(
                      [](const std::vector<uint8_t>& prefix,
                         const std::vector<uint8_t>& new_prefix,
                         const storage::DomStorageDatabase& db) {
                        WriteBatch batch;
                        Status status =
                            db.CopyPrefixed(prefix, new_prefix, &batch);
                        if (!status.ok())
                          return status;
                        return db.Commit(&batch);
                      },
                      source_key_prefix, destination_key_prefix),
                  std::move(callback));
}

void LevelDBDatabaseImpl::OnDatabaseOpened(
    StatusCallback callback,
    base::SequenceBound<storage::DomStorageDatabase> database,
    leveldb::Status status) {
  database_ = std::move(database);
  std::vector<DatabaseTask> tasks;
  std::swap(tasks, tasks_to_run_on_open_);
  if (status.ok()) {
    for (auto& task : tasks)
      database_.PostTaskWithThisObject(FROM_HERE, std::move(task));
  }
  std::move(callback).Run(status);
}

}  // namespace leveldb
