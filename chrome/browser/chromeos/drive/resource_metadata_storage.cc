// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace drive {
namespace internal {

namespace {

// Enum to describe DB initialization status.
enum DBInitStatus {
  DB_INIT_SUCCESS,
  DB_INIT_NOT_FOUND,
  DB_INIT_CORRUPTION,
  DB_INIT_IO_ERROR,
  DB_INIT_FAILED,
  DB_INIT_INCOMPATIBLE,
  DB_INIT_BROKEN,
  DB_INIT_OPENED_EXISTING_DB,
  DB_INIT_CREATED_NEW_DB,
  DB_INIT_REPLACED_EXISTING_DB_WITH_NEW_DB,
  DB_INIT_MAX_VALUE,
};

// The name of the DB which stores the metadata.
const base::FilePath::CharType kResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_resource_map.db");

// The name of the DB which couldn't be opened, but is preserved just in case.
const base::FilePath::CharType kPreservedResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_preserved_resource_map.db");

// The name of the DB which couldn't be opened, and was replaced with a new one.
const base::FilePath::CharType kTrashedResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_trashed_resource_map.db");

// Meant to be a character which never happen to be in real IDs.
const char kDBKeyDelimeter = '\0';

// String used as a suffix of a key for a cache entry.
const char kCacheEntryKeySuffix[] = "CACHE";

// String used as a prefix of a key for a resource-ID-to-local-ID entry.
const char kIdEntryKeyPrefix[] = "ID";

// Returns a string to be used as the key for the header.
std::string GetHeaderDBKey() {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append("HEADER");
  return key;
}

// Returns true if |key| is a key for a child entry.
bool IsChildEntryKey(const leveldb::Slice& key) {
  return !key.empty() && key[key.size() - 1] == kDBKeyDelimeter;
}

// Returns true if |key| is a key for a cache entry.
bool IsCacheEntryKey(const leveldb::Slice& key) {
  // A cache entry key should end with |kDBKeyDelimeter + kCacheEntryKeySuffix|.
  const leveldb::Slice expected_suffix(kCacheEntryKeySuffix,
                                       arraysize(kCacheEntryKeySuffix) - 1);
  if (key.size() < 1 + expected_suffix.size() ||
      key[key.size() - expected_suffix.size() - 1] != kDBKeyDelimeter)
    return false;

  const leveldb::Slice key_substring(
      key.data() + key.size() - expected_suffix.size(), expected_suffix.size());
  return key_substring.compare(expected_suffix) == 0;
}

// Returns ID extracted from a cache entry key.
std::string GetIdFromCacheEntryKey(const leveldb::Slice& key) {
  DCHECK(IsCacheEntryKey(key));
  // Drop the suffix |kDBKeyDelimeter + kCacheEntryKeySuffix| from the key.
  const size_t kSuffixLength = arraysize(kCacheEntryKeySuffix) - 1;
  const int id_length = key.size() - 1 - kSuffixLength;
  return std::string(key.data(), id_length);
}

// Returns a string to be used as a key for a resource-ID-to-local-ID entry.
std::string GetIdEntryKey(const std::string& resource_id) {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append(kIdEntryKeyPrefix);
  key.push_back(kDBKeyDelimeter);
  key.append(resource_id);
  return key;
}

// Returns true if |key| is a key for a resource-ID-to-local-ID entry.
bool IsIdEntryKey(const leveldb::Slice& key) {
  // A resource-ID-to-local-ID entry key should start with
  // |kDBKeyDelimeter + kIdEntryKeyPrefix + kDBKeyDelimeter|.
  const leveldb::Slice expected_prefix(kIdEntryKeyPrefix,
                                       arraysize(kIdEntryKeyPrefix) - 1);
  if (key.size() < 2 + expected_prefix.size())
    return false;
  const leveldb::Slice key_substring(key.data() + 1, expected_prefix.size());
  return key[0] == kDBKeyDelimeter &&
      key_substring.compare(expected_prefix) == 0 &&
      key[expected_prefix.size() + 1] == kDBKeyDelimeter;
}

// Returns the resource ID extracted from a resource-ID-to-local-ID entry key.
std::string GetResourceIdFromIdEntryKey(const leveldb::Slice& key) {
  DCHECK(IsIdEntryKey(key));
  // Drop the prefix |kDBKeyDelimeter + kIdEntryKeyPrefix + kDBKeyDelimeter|
  // from the key.
  const size_t kPrefixLength = arraysize(kIdEntryKeyPrefix) - 1;
  const int offset = kPrefixLength + 2;
  return std::string(key.data() + offset, key.size() - offset);
}

// Converts leveldb::Status to DBInitStatus.
DBInitStatus LevelDBStatusToDBInitStatus(const leveldb::Status& status) {
  if (status.ok())
    return DB_INIT_SUCCESS;
  if (status.IsNotFound())
    return DB_INIT_NOT_FOUND;
  if (status.IsCorruption())
    return DB_INIT_CORRUPTION;
  if (status.IsIOError())
    return DB_INIT_IO_ERROR;
  return DB_INIT_FAILED;
}

// Converts leveldb::Status to FileError.
FileError LevelDBStatusToFileError(const leveldb::Status& status) {
  if (status.ok())
    return FILE_ERROR_OK;
  if (status.IsNotFound())
    return FILE_ERROR_NOT_FOUND;
  if (leveldb_env::IndicatesDiskFull(status))
    return FILE_ERROR_NO_LOCAL_SPACE;
  return FILE_ERROR_FAILED;
}

ResourceMetadataHeader GetDefaultHeaderEntry() {
  ResourceMetadataHeader header;
  header.set_version(ResourceMetadataStorage::kDBVersion);
  return header;
}

bool MoveIfPossible(const base::FilePath& from, const base::FilePath& to) {
  return !base::PathExists(from) || base::Move(from, to);
}

}  // namespace

ResourceMetadataStorage::Iterator::Iterator(scoped_ptr<leveldb::Iterator> it)
  : it_(it.Pass()) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(it_);

  // Skip the header entry.
  // Note: The header entry comes before all other entries because its key
  // starts with kDBKeyDelimeter. (i.e. '\0')
  it_->Seek(leveldb::Slice(GetHeaderDBKey()));

  Advance();
}

ResourceMetadataStorage::Iterator::~Iterator() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool ResourceMetadataStorage::Iterator::IsAtEnd() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->Valid();
}

std::string ResourceMetadataStorage::Iterator::GetID() const {
  return it_->key().ToString();
}

const ResourceEntry& ResourceMetadataStorage::Iterator::GetValue() const {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());
  return entry_;
}

void ResourceMetadataStorage::Iterator::Advance() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());

  for (it_->Next() ; it_->Valid(); it_->Next()) {
    if (!IsChildEntryKey(it_->key()) &&
        !IsIdEntryKey(it_->key()) &&
        entry_.ParseFromArray(it_->value().data(), it_->value().size())) {
      break;
    }
  }
}

bool ResourceMetadataStorage::Iterator::HasError() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->status().ok();
}

// static
bool ResourceMetadataStorage::UpgradeOldDB(
    const base::FilePath& directory_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  COMPILE_ASSERT(
      kDBVersion == 13,
      db_version_and_this_function_should_be_updated_at_the_same_time);

  const base::FilePath resource_map_path =
      directory_path.Append(kResourceMapDBName);
  const base::FilePath preserved_resource_map_path =
      directory_path.Append(kPreservedResourceMapDBName);

  if (base::PathExists(preserved_resource_map_path)) {
    // Preserved DB is found. The previous attempt to create a new DB should not
    // be successful. Discard the imperfect new DB and restore the old DB.
    if (!base::DeleteFile(resource_map_path, false /* recursive */) ||
        !base::Move(preserved_resource_map_path, resource_map_path))
      return false;
  }

  if (!base::PathExists(resource_map_path))
    return false;

  // Open DB.
  leveldb::DB* db = NULL;
  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;
  if (!leveldb::DB::Open(options, resource_map_path.AsUTF8Unsafe(), &db).ok())
    return false;
  scoped_ptr<leveldb::DB> resource_map(db);

  // Check DB version.
  std::string serialized_header;
  ResourceMetadataHeader header;
  if (!resource_map->Get(leveldb::ReadOptions(),
                         leveldb::Slice(GetHeaderDBKey()),
                         &serialized_header).ok() ||
      !header.ParseFromString(serialized_header))
    return false;
  UMA_HISTOGRAM_SPARSE_SLOWLY("Drive.MetadataDBVersionBeforeUpgradeCheck",
                              header.version());

  if (header.version() == kDBVersion) {
    // Before r272134, UpgradeOldDB() was not deleting unused ID entries.
    // Delete unused ID entries to fix crbug.com/374648.
    std::set<std::string> used_ids;

    scoped_ptr<leveldb::Iterator> it(
        resource_map->NewIterator(leveldb::ReadOptions()));
    it->Seek(leveldb::Slice(GetHeaderDBKey()));
    it->Next();
    for (; it->Valid(); it->Next()) {
      if (IsCacheEntryKey(it->key())) {
        used_ids.insert(GetIdFromCacheEntryKey(it->key()));
      } else if (!IsChildEntryKey(it->key()) && !IsIdEntryKey(it->key())) {
        used_ids.insert(it->key().ToString());
      }
    }
    if (!it->status().ok())
      return false;

    leveldb::WriteBatch batch;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsIdEntryKey(it->key()) && !used_ids.count(it->value().ToString()))
        batch.Delete(it->key());
    }
    if (!it->status().ok())
      return false;

    return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
  } else if (header.version() < 6) {  // Too old, nothing can be done.
    return false;
  } else if (header.version() < 11) {  // Cache entries can be reused.
    leveldb::ReadOptions options;
    options.verify_checksums = true;
    scoped_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

    leveldb::WriteBatch batch;
    // First, remove all entries.
    for (it->SeekToFirst(); it->Valid(); it->Next())
      batch.Delete(it->key());

    // Put ID entries and cache entries.
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsCacheEntryKey(it->key())) {
        FileCacheEntry cache_entry;
        if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
          return false;

        // The resource ID might be in old WAPI format. We need to canonicalize
        // to the format of API service currently in use.
        const std::string& id = GetIdFromCacheEntryKey(it->key());
        const std::string& id_new = util::CanonicalizeResourceId(id);

        // Before v11, resource ID was directly used as local ID. Such entries
        // can be migrated by adding an identity ID mapping.
        batch.Put(GetIdEntryKey(id_new), id_new);

        // Put cache state into a ResourceEntry.
        ResourceEntry entry;
        entry.set_local_id(id_new);
        entry.set_resource_id(id_new);
        *entry.mutable_file_specific_info()->mutable_cache_state() =
            cache_entry;

        std::string serialized_entry;
        if (!entry.SerializeToString(&serialized_entry)) {
          DLOG(ERROR) << "Failed to serialize the entry: " << id;
          return false;
        }
        batch.Put(id_new, serialized_entry);
      }
    }
    if (!it->status().ok())
      return false;

    // Put header with the latest version number.
    std::string serialized_header;
    if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
      return false;
    batch.Put(GetHeaderDBKey(), serialized_header);

    return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
  } else if (header.version() < 12) {  // Cache and ID map entries are reusable.
    leveldb::ReadOptions options;
    options.verify_checksums = true;
    scoped_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

    // First, get the set of local IDs associated with cache entries.
    std::set<std::string> cached_entry_ids;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsCacheEntryKey(it->key()))
        cached_entry_ids.insert(GetIdFromCacheEntryKey(it->key()));
    }
    if (!it->status().ok())
      return false;

    // Remove all entries except used ID entries.
    leveldb::WriteBatch batch;
    std::map<std::string, std::string> local_id_to_resource_id;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      const bool is_used_id = IsIdEntryKey(it->key()) &&
          cached_entry_ids.count(it->value().ToString());
      if (is_used_id) {
        local_id_to_resource_id[it->value().ToString()] =
            GetResourceIdFromIdEntryKey(it->key());
      } else {
        batch.Delete(it->key());
      }
    }
    if (!it->status().ok())
      return false;

    // Put cache entries.
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsCacheEntryKey(it->key())) {
        const std::string& id = GetIdFromCacheEntryKey(it->key());

        std::map<std::string, std::string>::const_iterator iter_resource_id =
            local_id_to_resource_id.find(id);
        if (iter_resource_id == local_id_to_resource_id.end())
          continue;

        FileCacheEntry cache_entry;
        if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
          return false;

        // Put cache state into a ResourceEntry.
        ResourceEntry entry;
        entry.set_local_id(id);
        entry.set_resource_id(iter_resource_id->second);
        *entry.mutable_file_specific_info()->mutable_cache_state() =
            cache_entry;

        std::string serialized_entry;
        if (!entry.SerializeToString(&serialized_entry)) {
          DLOG(ERROR) << "Failed to serialize the entry: " << id;
          return false;
        }
        batch.Put(id, serialized_entry);
      }
    }
    if (!it->status().ok())
      return false;

    // Put header with the latest version number.
    std::string serialized_header;
    if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
      return false;
    batch.Put(GetHeaderDBKey(), serialized_header);

    return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
  } else if (header.version() < 13) {  // Reuse all entries.
    leveldb::ReadOptions options;
    options.verify_checksums = true;
    scoped_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

    // First, get local ID to resource ID map.
    std::map<std::string, std::string> local_id_to_resource_id;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsIdEntryKey(it->key())) {
        local_id_to_resource_id[it->value().ToString()] =
            GetResourceIdFromIdEntryKey(it->key());
      }
    }
    if (!it->status().ok())
      return false;

    leveldb::WriteBatch batch;
    // Merge cache entries to ResourceEntry.
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (IsCacheEntryKey(it->key())) {
        const std::string& id = GetIdFromCacheEntryKey(it->key());

        FileCacheEntry cache_entry;
        if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
          return false;

        std::string serialized_entry;
        leveldb::Status status = resource_map->Get(options,
                                                   leveldb::Slice(id),
                                                   &serialized_entry);

        std::map<std::string, std::string>::const_iterator iter_resource_id =
            local_id_to_resource_id.find(id);

        // No need to keep cache-only entries without resource ID.
        if (status.IsNotFound() &&
            iter_resource_id == local_id_to_resource_id.end())
          continue;

        ResourceEntry entry;
        if (status.ok()) {
          if (!entry.ParseFromString(serialized_entry))
            return false;
        } else if (status.IsNotFound()) {
          entry.set_local_id(id);
          entry.set_resource_id(iter_resource_id->second);
        } else {
          DLOG(ERROR) << "Failed to get the entry: " << id;
          return false;
        }
        *entry.mutable_file_specific_info()->mutable_cache_state() =
            cache_entry;

        if (!entry.SerializeToString(&serialized_entry)) {
          DLOG(ERROR) << "Failed to serialize the entry: " << id;
          return false;
        }
        batch.Delete(it->key());
        batch.Put(id, serialized_entry);
      }
    }
    if (!it->status().ok())
      return false;

    // Put header with the latest version number.
    header.set_version(ResourceMetadataStorage::kDBVersion);
    std::string serialized_header;
    if (!header.SerializeToString(&serialized_header))
      return false;
    batch.Put(GetHeaderDBKey(), serialized_header);

    return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
  }

  LOG(WARNING) << "Unexpected DB version: " << header.version();
  return false;
}

ResourceMetadataStorage::ResourceMetadataStorage(
    const base::FilePath& directory_path,
    base::SequencedTaskRunner* blocking_task_runner)
    : directory_path_(directory_path),
      cache_file_scan_is_needed_(true),
      blocking_task_runner_(blocking_task_runner) {
}

void ResourceMetadataStorage::Destroy() {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ResourceMetadataStorage::DestroyOnBlockingPool,
                 base::Unretained(this)));
}

bool ResourceMetadataStorage::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();

  resource_map_.reset();

  const base::FilePath resource_map_path =
      directory_path_.Append(kResourceMapDBName);
  const base::FilePath preserved_resource_map_path =
      directory_path_.Append(kPreservedResourceMapDBName);
  const base::FilePath trashed_resource_map_path =
      directory_path_.Append(kTrashedResourceMapDBName);

  // Discard unneeded DBs.
  if (!base::DeleteFile(preserved_resource_map_path, true /* recursive */) ||
      !base::DeleteFile(trashed_resource_map_path, true /* recursive */)) {
    LOG(ERROR) << "Failed to remove unneeded DBs.";
    return false;
  }

  // Try to open the existing DB.
  leveldb::DB* db = NULL;
  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;

  DBInitStatus open_existing_result = DB_INIT_NOT_FOUND;
  leveldb::Status status;
  if (base::PathExists(resource_map_path)) {
    status = leveldb::DB::Open(options, resource_map_path.AsUTF8Unsafe(), &db);
    open_existing_result = LevelDBStatusToDBInitStatus(status);
  }

  if (open_existing_result == DB_INIT_SUCCESS) {
    resource_map_.reset(db);

    // Check the validity of existing DB.
    int db_version = -1;
    ResourceMetadataHeader header;
    if (GetHeader(&header) == FILE_ERROR_OK)
      db_version = header.version();

    bool should_discard_db = true;
    if (db_version != kDBVersion) {
      open_existing_result = DB_INIT_INCOMPATIBLE;
      DVLOG(1) << "Reject incompatible DB.";
    } else if (!CheckValidity()) {
      open_existing_result = DB_INIT_BROKEN;
      LOG(ERROR) << "Reject invalid DB.";
    } else {
      should_discard_db = false;
    }

    if (should_discard_db)
      resource_map_.reset();
    else
      cache_file_scan_is_needed_ = false;
  }

  UMA_HISTOGRAM_ENUMERATION("Drive.MetadataDBOpenExistingResult",
                            open_existing_result,
                            DB_INIT_MAX_VALUE);

  DBInitStatus init_result = DB_INIT_OPENED_EXISTING_DB;

  // Failed to open the existing DB, create new DB.
  if (!resource_map_) {
    // Move the existing DB to the preservation path. The moved old DB is
    // deleted once the new DB creation succeeds, or is restored later in
    // UpgradeOldDB() when the creation fails.
    MoveIfPossible(resource_map_path, preserved_resource_map_path);

    // Create DB.
    options.max_open_files = 0;  // Use minimum.
    options.create_if_missing = true;
    options.error_if_exists = true;

    status = leveldb::DB::Open(options, resource_map_path.AsUTF8Unsafe(), &db);
    if (status.ok()) {
      resource_map_.reset(db);

      // Set up header and trash the old DB.
      if (PutHeader(GetDefaultHeaderEntry()) == FILE_ERROR_OK &&
          MoveIfPossible(preserved_resource_map_path,
                         trashed_resource_map_path)) {
        init_result = open_existing_result == DB_INIT_NOT_FOUND ?
            DB_INIT_CREATED_NEW_DB : DB_INIT_REPLACED_EXISTING_DB_WITH_NEW_DB;
      } else {
        init_result = DB_INIT_FAILED;
        resource_map_.reset();
      }
    } else {
      LOG(ERROR) << "Failed to create resource map DB: " << status.ToString();
      init_result = LevelDBStatusToDBInitStatus(status);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Drive.MetadataDBInitResult",
                            init_result,
                            DB_INIT_MAX_VALUE);
  return resource_map_;
}

void ResourceMetadataStorage::RecoverCacheInfoFromTrashedResourceMap(
    RecoveredCacheInfoMap* out_info) {
  const base::FilePath trashed_resource_map_path =
      directory_path_.Append(kTrashedResourceMapDBName);

  if (!base::PathExists(trashed_resource_map_path))
    return;

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;

  // Trashed DB may be broken, repair it first.
  leveldb::Status status;
  status = leveldb::RepairDB(trashed_resource_map_path.AsUTF8Unsafe(), options);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to repair trashed DB: " << status.ToString();
    return;
  }

  // Open it.
  leveldb::DB* db = NULL;
  status = leveldb::DB::Open(options, trashed_resource_map_path.AsUTF8Unsafe(),
                             &db);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open trashed DB: " << status.ToString();
    return;
  }
  scoped_ptr<leveldb::DB> resource_map(db);

  // Check DB version.
  std::string serialized_header;
  ResourceMetadataHeader header;
  if (!resource_map->Get(leveldb::ReadOptions(),
                         leveldb::Slice(GetHeaderDBKey()),
                         &serialized_header).ok() ||
      !header.ParseFromString(serialized_header) ||
      header.version() != kDBVersion) {
    LOG(ERROR) << "Incompatible DB version: " << header.version();
    return;
  }

  // Collect cache entries.
  scoped_ptr<leveldb::Iterator> it(
      resource_map->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (!IsChildEntryKey(it->key()) &&
        !IsIdEntryKey(it->key())) {
      const std::string id = it->key().ToString();
      ResourceEntry entry;
      if (entry.ParseFromArray(it->value().data(), it->value().size()) &&
          entry.file_specific_info().has_cache_state()) {
        RecoveredCacheInfo* info = &(*out_info)[id];
        info->is_dirty = entry.file_specific_info().cache_state().is_dirty();
        info->md5 = entry.file_specific_info().cache_state().md5();
        info->title = entry.title();
      }
    }
  }
}

FileError ResourceMetadataStorage::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();

  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  header.set_largest_changestamp(largest_changestamp);
  return PutHeader(header);
}

FileError ResourceMetadataStorage::GetLargestChangestamp(
    int64* largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();
  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  *largest_changestamp = header.largest_changestamp();
  return FILE_ERROR_OK;
}

FileError ResourceMetadataStorage::PutEntry(const ResourceEntry& entry) {
  base::ThreadRestrictions::AssertIOAllowed();

  const std::string& id = entry.local_id();
  DCHECK(!id.empty());

  // Try to get existing entry.
  std::string serialized_entry;
  leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                              leveldb::Slice(id),
                                              &serialized_entry);
  if (!status.ok() && !status.IsNotFound())  // Unexpected errors.
    return LevelDBStatusToFileError(status);

  ResourceEntry old_entry;
  if (status.ok() && !old_entry.ParseFromString(serialized_entry))
    return FILE_ERROR_FAILED;

  // Construct write batch.
  leveldb::WriteBatch batch;

  // Remove from the old parent.
  if (!old_entry.parent_local_id().empty()) {
    batch.Delete(GetChildEntryKey(old_entry.parent_local_id(),
                                  old_entry.base_name()));
  }
  // Add to the new parent.
  if (!entry.parent_local_id().empty())
    batch.Put(GetChildEntryKey(entry.parent_local_id(), entry.base_name()), id);

  // Refresh resource-ID-to-local-ID mapping entry.
  if (old_entry.resource_id() != entry.resource_id()) {
    // Resource ID should not change.
    DCHECK(old_entry.resource_id().empty() || entry.resource_id().empty());

    if (!old_entry.resource_id().empty())
      batch.Delete(GetIdEntryKey(old_entry.resource_id()));
    if (!entry.resource_id().empty())
      batch.Put(GetIdEntryKey(entry.resource_id()), id);
  }

  // Put the entry itself.
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry: " << id;
    return FILE_ERROR_FAILED;
  }
  batch.Put(id, serialized_entry);

  status = resource_map_->Write(leveldb::WriteOptions(), &batch);
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetEntry(const std::string& id,
                                            ResourceEntry* out_entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                                    leveldb::Slice(id),
                                                    &serialized_entry);
  if (!status.ok())
    return LevelDBStatusToFileError(status);
  if (!out_entry->ParseFromString(serialized_entry))
    return FILE_ERROR_FAILED;
  return FILE_ERROR_OK;
}

FileError ResourceMetadataStorage::RemoveEntry(const std::string& id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  ResourceEntry entry;
  FileError error = GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  leveldb::WriteBatch batch;

  // Remove from the parent.
  if (!entry.parent_local_id().empty())
    batch.Delete(GetChildEntryKey(entry.parent_local_id(), entry.base_name()));

  // Remove resource ID-local ID mapping entry.
  if (!entry.resource_id().empty())
    batch.Delete(GetIdEntryKey(entry.resource_id()));

  // Remove the entry itself.
  batch.Delete(id);

  const leveldb::Status status = resource_map_->Write(leveldb::WriteOptions(),
                                                      &batch);
  return LevelDBStatusToFileError(status);
}

scoped_ptr<ResourceMetadataStorage::Iterator>
ResourceMetadataStorage::GetIterator() {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  return make_scoped_ptr(new Iterator(it.Pass()));
}

FileError ResourceMetadataStorage::GetChild(const std::string& parent_id,
                                            const std::string& child_name,
                                            std::string* child_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!parent_id.empty());
  DCHECK(!child_name.empty());

  const leveldb::Status status =
      resource_map_->Get(
          leveldb::ReadOptions(),
          leveldb::Slice(GetChildEntryKey(parent_id, child_name)),
          child_id);
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetChildren(
    const std::string& parent_id,
    std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!parent_id.empty());

  // Iterate over all entries with keys starting with |parent_id|.
  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  for (it->Seek(parent_id);
       it->Valid() && it->key().starts_with(leveldb::Slice(parent_id));
       it->Next()) {
    if (IsChildEntryKey(it->key()))
      children->push_back(it->value().ToString());
  }
  return LevelDBStatusToFileError(it->status());
}

ResourceMetadataStorage::RecoveredCacheInfo::RecoveredCacheInfo()
    : is_dirty(false) {}

ResourceMetadataStorage::RecoveredCacheInfo::~RecoveredCacheInfo() {}

FileError ResourceMetadataStorage::GetIdByResourceId(
    const std::string& resource_id,
    std::string* out_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetIdEntryKey(resource_id)),
      out_id);
  return LevelDBStatusToFileError(status);
}

ResourceMetadataStorage::~ResourceMetadataStorage() {
  base::ThreadRestrictions::AssertIOAllowed();
}

void ResourceMetadataStorage::DestroyOnBlockingPool() {
  delete this;
}

// static
std::string ResourceMetadataStorage::GetChildEntryKey(
    const std::string& parent_id,
    const std::string& child_name) {
  DCHECK(!parent_id.empty());
  DCHECK(!child_name.empty());

  std::string key = parent_id;
  key.push_back(kDBKeyDelimeter);
  key.append(child_name);
  key.push_back(kDBKeyDelimeter);
  return key;
}

FileError ResourceMetadataStorage::PutHeader(
    const ResourceMetadataHeader& header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  if (!header.SerializeToString(&serialized_header)) {
    DLOG(ERROR) << "Failed to serialize the header";
    return FILE_ERROR_FAILED;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      leveldb::Slice(serialized_header));
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetHeader(ResourceMetadataHeader* header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      &serialized_header);
  if (!status.ok())
    return LevelDBStatusToFileError(status);
  return header->ParseFromString(serialized_header) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

bool ResourceMetadataStorage::CheckValidity() {
  base::ThreadRestrictions::AssertIOAllowed();

  // Perform read with checksums verification enalbed.
  leveldb::ReadOptions options;
  options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> it(resource_map_->NewIterator(options));
  it->SeekToFirst();

  // DB is organized like this:
  //
  // <key>                          : <value>
  // "\0HEADER"                     : ResourceMetadataHeader
  // "\0ID\0|resource ID 1|"        : Local ID associated to resource ID 1.
  // "\0ID\0|resource ID 2|"        : Local ID associated to resource ID 2.
  // ...
  // "|ID of A|"                    : ResourceEntry for entry A.
  // "|ID of A|\0|child name 1|\0"  : ID of the 1st child entry of entry A.
  // "|ID of A|\0|child name 2|\0"  : ID of the 2nd child entry of entry A.
  // ...
  // "|ID of A|\0|child name n|\0"  : ID of the nth child entry of entry A.
  // "|ID of B|"                    : ResourceEntry for entry B.
  // ...

  // Check the header.
  ResourceMetadataHeader header;
  if (!it->Valid() ||
      it->key() != GetHeaderDBKey() ||  // Header entry must come first.
      !header.ParseFromArray(it->value().data(), it->value().size()) ||
      header.version() != kDBVersion) {
    DLOG(ERROR) << "Invalid header detected. version = " << header.version();
    return false;
  }

  // Check all entries.
  size_t num_entries_with_parent = 0;
  size_t num_child_entries = 0;
  ResourceEntry entry;
  std::string serialized_entry;
  std::string child_id;
  for (it->Next(); it->Valid(); it->Next()) {
    // Count child entries.
    if (IsChildEntryKey(it->key())) {
      ++num_child_entries;
      continue;
    }

    // Check if resource-ID-to-local-ID mapping is stored correctly.
    if (IsIdEntryKey(it->key())) {
      leveldb::Status status = resource_map_->Get(
          options, it->value(), &serialized_entry);
      // Resource-ID-to-local-ID mapping without entry for the local ID is ok.
      if (status.IsNotFound())
        continue;
      // When the entry exists, its resource ID must be consistent.
      const bool ok = status.ok() &&
          entry.ParseFromString(serialized_entry) &&
          !entry.resource_id().empty() &&
          leveldb::Slice(GetIdEntryKey(entry.resource_id())) == it->key();
      if (!ok) {
        DLOG(ERROR) << "Broken ID entry. status = " << status.ToString();
        return false;
      }
      continue;
    }

    // Check if stored data is broken.
    if (!entry.ParseFromArray(it->value().data(), it->value().size())) {
      DLOG(ERROR) << "Broken entry detected";
      return false;
    }

    if (leveldb::Slice(entry.local_id()) != it->key()) {
      DLOG(ERROR) << "Wrong local ID.";
      return false;
    }

    if (!entry.parent_local_id().empty()) {
      // Check if the parent entry is stored.
      leveldb::Status status = resource_map_->Get(
          options,
          leveldb::Slice(entry.parent_local_id()),
          &serialized_entry);
      if (!status.ok()) {
        DLOG(ERROR) << "Can't get parent entry. status = " << status.ToString();
        return false;
      }

      // Check if parent-child relationship is stored correctly.
      status = resource_map_->Get(
          options,
          leveldb::Slice(GetChildEntryKey(entry.parent_local_id(),
                                          entry.base_name())),
          &child_id);
      if (!status.ok() || leveldb::Slice(child_id) != it->key()) {
        DLOG(ERROR) << "Child map is broken. status = " << status.ToString();
        return false;
      }
      ++num_entries_with_parent;
    }
  }
  if (!it->status().ok() || num_child_entries != num_entries_with_parent) {
    DLOG(ERROR) << "Error during checking resource map. status = "
                << it->status().ToString();
    return false;
  }
  return true;
}

}  // namespace internal
}  // namespace drive
