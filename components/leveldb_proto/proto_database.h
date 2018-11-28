// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_

#include <map>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

// Interface for classes providing persistent storage of Protocol Buffer
// entries (T must be a Proto type extending MessageLite).
template <typename T>
class ProtoDatabase {
 public:
  using InitCallback = base::OnceCallback<void(bool success)>;
  using UpdateCallback = base::OnceCallback<void(bool success)>;
  using LoadCallback =
      base::OnceCallback<void(bool success, std::unique_ptr<std::vector<T>>)>;
  using LoadKeysCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::vector<std::string>>)>;
  using LoadKeysAndEntriesCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::map<std::string, T>>)>;
  using GetCallback =
      base::OnceCallback<void(bool success, std::unique_ptr<T>)>;
  using DestroyCallback = base::OnceCallback<void(bool success)>;

  // A list of key-value (string, T) tuples.
  using KeyEntryVector = std::vector<std::pair<std::string, T>>;

  virtual ~ProtoDatabase() = default;

  // Asynchronously initializes the object with the specified |options|.
  // |callback| will be invoked on the calling thread when complete.
  virtual void Init(const std::string& client_name, InitCallback callback) = 0;
  // This version of Init is for compatibility, since many of the current
  // proto database clients still use this.
  virtual void Init(const char* client_name,
                    const base::FilePath& database_dir,
                    const leveldb_env::Options& options,
                    InitCallback callback) = 0;

  virtual void InitWithDatabase(LevelDB* database,
                                const base::FilePath& database_dir,
                                const leveldb_env::Options& options,
                                InitCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete.
  virtual void UpdateEntries(
      std::unique_ptr<KeyEntryVector> entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      UpdateCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries that satisfies
  // the |delete_key_filter| from the database. |callback| will be invoked on
  // the calling thread when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyEntryVector> entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      UpdateCallback callback) = 0;
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyEntryVector> entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      UpdateCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(LoadCallback callback) = 0;

  // Asynchronously loads entries that satisfies the |filter| from the database
  // and invokes |callback| when complete. The filter will be called on
  // ProtoDatabase's taskrunner.
  virtual void LoadEntriesWithFilter(const LevelDB::KeyFilter& filter,
                                     LoadCallback callback) = 0;
  virtual void LoadEntriesWithFilter(const LevelDB::KeyFilter& key_filter,
                                     const leveldb::ReadOptions& options,
                                     const std::string& target_prefix,
                                     LoadCallback callback) = 0;

  virtual void LoadKeysAndEntries(LoadKeysAndEntriesCallback callback) = 0;

  virtual void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      LoadKeysAndEntriesCallback callback) = 0;
  virtual void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      LoadKeysAndEntriesCallback callback) = 0;

  // Asynchronously loads all keys from the database and invokes |callback| with
  // those keys when complete.
  virtual void LoadKeys(LoadKeysCallback callback) = 0;
  virtual void LoadKeys(const std::string& target_prefix,
                        LoadKeysCallback callback) = 0;

  // Asynchronously loads a single entry, identified by |key|, from the database
  // and invokes |callback| when complete. If no entry with |key| is found,
  // a nullptr is passed to the callback, but the success flag is still true.
  virtual void GetEntry(const std::string& key, GetCallback callback) = 0;

  // Asynchronously destroys the database.
  virtual void Destroy(DestroyCallback callback) = 0;

 protected:
  ProtoDatabase() = default;
};

// Return a new instance of Options, but with two additions:
// 1) create_if_missing = true
// 2) max_open_files = 0
leveldb_env::Options CreateSimpleOptions();

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_