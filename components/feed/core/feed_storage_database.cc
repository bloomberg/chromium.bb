// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_storage_database.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "components/feed/core/proto/feed_storage.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feed {

namespace {
using StorageEntryVector =
    leveldb_proto::ProtoDatabase<FeedStorageProto>::KeyEntryVector;

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.FeedStorageDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kStorageDatabaseUMAClientName[] = "FeedStorageDatabase";

const char kStorageDatabaseFolder[] = "storage";

const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;

// Key prefix for content storage.
const char kContentStoragePrefix[] = "cs-";

// Formats key prefix for content data's key.
std::string FormatContentDatabaseKey(const std::string& key) {
  return kContentStoragePrefix + key;
}

bool DatabaseKeyFilter(const std::unordered_set<std::string>& key_set,
                       const std::string& key) {
  return key_set.find(key) != key_set.end();
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

FeedStorageDatabase::FeedStorageDatabase(
    const base::FilePath& database_folder,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : FeedStorageDatabase(
          database_folder,
          std::make_unique<leveldb_proto::ProtoDatabaseImpl<FeedStorageProto>>(
              task_runner)) {}

FeedStorageDatabase::FeedStorageDatabase(
    const base::FilePath& database_folder,
    std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
        storage_database)
    : database_status_(UNINITIALIZED),
      storage_database_(std::move(storage_database)),
      weak_ptr_factory_(this) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  if (base::SysInfo::IsLowEndDevice()) {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  } else {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  }

  base::FilePath storage_folder =
      database_folder.AppendASCII(kStorageDatabaseFolder);
  storage_database_->Init(
      kStorageDatabaseUMAClientName, storage_folder, options,
      base::BindOnce(&FeedStorageDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

FeedStorageDatabase::~FeedStorageDatabase() = default;

bool FeedStorageDatabase::IsInitialized() const {
  return INITIALIZED == database_status_;
}

void FeedStorageDatabase::LoadContentEntries(
    const std::vector<std::string>& keys,
    FeedContentStorageDatabaseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unordered_set<std::string> key_set;
  for (const auto& key : keys) {
    key_set.insert(FormatContentDatabaseKey(key));
  }

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabaseKeyFilter, std::move(key_set)),
      base::BindOnce(&FeedStorageDatabase::OnContentEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::LoadContentEntriesByPrefix(
    const std::string& prefix,
    FeedContentStorageDatabaseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatContentDatabaseKey(prefix);

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnContentEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::SaveContentEntries(
    std::vector<KeyAndData> entries,
    FeedStorageCommitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto entries_to_save = std::make_unique<StorageEntryVector>();
  for (const auto& entry : entries) {
    FeedStorageProto proto;
    proto.set_key(entry.first);
    proto.set_content_data(entry.second);
    entries_to_save->emplace_back(FormatContentDatabaseKey(entry.first),
                                  std::move(proto));
  }

  storage_database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteContentEntries(
    std::vector<std::string> keys_to_delete,
    FeedStorageCommitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unordered_set<std::string> key_set;
  for (const auto& key : keys_to_delete) {
    key_set.insert(FormatContentDatabaseKey(key));
  }
  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabaseKeyFilter, std::move(key_set)),
      base::BindOnce(&FeedStorageDatabase::OnContentDeletedEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteContentEntriesByPrefix(
    const std::string& prefix_to_delete,
    FeedStorageCommitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatContentDatabaseKey(prefix_to_delete);
  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnContentDeletedEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::OnDatabaseInitialized(bool success) {
  DCHECK_EQ(database_status_, UNINITIALIZED);

  if (success) {
    database_status_ = INITIALIZED;
  } else {
    database_status_ = INIT_FAILURE;
    DVLOG(1) << "FeedStorageDatabase init failed.";
  }
}

void FeedStorageDatabase::OnContentEntriesLoaded(
    FeedContentStorageDatabaseCallback callback,
    bool success,
    std::unique_ptr<std::vector<FeedStorageProto>> entries) {
  std::vector<KeyAndData> results;

  if (!success || !entries) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load content failed.";
    std::move(callback).Run(std::move(results));
    return;
  }

  for (const auto& entry : *entries) {
    DCHECK(entry.has_key());
    DCHECK(entry.has_content_data());

    results.emplace_back(std::make_pair(entry.key(), entry.content_data()));
  }

  std::move(callback).Run(std::move(results));
}

void FeedStorageDatabase::OnContentDeletedEntriesLoaded(
    FeedStorageCommitCallback callback,
    bool success,
    std::unique_ptr<std::vector<FeedStorageProto>> entries) {
  auto entries_to_delete = std::make_unique<std::vector<std::string>>();

  if (!success || !entries) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load content failed.";
    std::move(callback).Run(success);
    return;
  }

  for (const auto& entry : *entries) {
    DCHECK(entry.has_content_data());
    entries_to_delete->push_back(FormatContentDatabaseKey(entry.key()));
  }

  storage_database_->UpdateEntries(
      std::make_unique<StorageEntryVector>(), std::move(entries_to_delete),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::OnStorageCommitted(FeedStorageCommitCallback callback,
                                             bool success) {
  DVLOG_IF(1, !success) << "FeedStorageDatabase committed failed.";
  std::move(callback).Run(success);
}

}  // namespace feed
