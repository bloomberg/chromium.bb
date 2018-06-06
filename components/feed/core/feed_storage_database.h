// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database.h"

namespace feed {

class FeedStorageProto;

// FeedStorageDatabase is leveldb backed store for feed's content storage data
// and jounal storage data.
class FeedStorageDatabase {
 public:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    INIT_FAILURE,
  };

  using KeyAndData = std::pair<std::string, std::string>;

  // Returns the storage data as a vector of key-value pairs when calling
  // loading data.
  using FeedContentStorageDatabaseCallback =
      base::OnceCallback<void(std::vector<KeyAndData>)>;

  // Returns the commit operations success or not.
  using FeedStorageCommitCallback = base::OnceCallback<void(bool)>;

  // Initializes the database with |database_folder|.
  FeedStorageDatabase(
      const base::FilePath& database_folder,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Initializes the database with |database_folder|. Creates storage using the
  // given |storage_database| for local storage. Useful for testing.
  FeedStorageDatabase(
      const base::FilePath& database_folder,
      std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
          storage_database);

  ~FeedStorageDatabase();

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may already started, or initialization
  // failed.
  bool IsInitialized() const;

  // content storage.
  void LoadContentEntries(const std::vector<std::string>& keys,
                          FeedContentStorageDatabaseCallback callback);
  void LoadContentEntriesByPrefix(const std::string& prefix,
                                  FeedContentStorageDatabaseCallback callback);
  void SaveContentEntries(std::vector<KeyAndData> entries,
                          FeedStorageCommitCallback callback);
  void DeleteContentEntries(std::vector<std::string> keys_to_delete,
                            FeedStorageCommitCallback callback);
  void DeleteContentEntriesByPrefix(const std::string& prefix_to_delete,
                                    FeedStorageCommitCallback callback);

 private:
  // Initialization
  void OnDatabaseInitialized(bool success);

  // Loading
  void OnContentEntriesLoaded(
      FeedContentStorageDatabaseCallback callback,
      bool success,
      std::unique_ptr<std::vector<FeedStorageProto>> entries);

  // Deleting
  void OnContentDeletedEntriesLoaded(
      FeedStorageCommitCallback callback,
      bool success,
      std::unique_ptr<std::vector<FeedStorageProto>> entries);

  // Commit callback
  void OnStorageCommitted(FeedStorageCommitCallback callback, bool success);

  State database_status_;

  std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
      storage_database_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeedStorageDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedStorageDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_
