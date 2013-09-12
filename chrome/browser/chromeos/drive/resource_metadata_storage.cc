// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
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
  DB_INIT_MAX_VALUE,
};

const base::FilePath::CharType kResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_resource_map.db");

// Meant to be a character which never happen to be in real IDs.
const char kDBKeyDelimeter = '\0';

// String used as a suffix of a key for a cache entry.
const char kCacheEntryKeySuffix[] = "CACHE";

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

// Returns a string to be used as a key for a cache entry.
std::string GetCacheEntryKey(const std::string& id) {
  std::string key(id);
  key.push_back(kDBKeyDelimeter);
  key.append(kCacheEntryKeySuffix);
  return key;
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

// Converts leveldb::Status to DBInitStatus.
DBInitStatus LevelDBStatusToDBInitStatus(const leveldb::Status status) {
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

// Returns true when the status indicates an unrecoverable error.
bool IsUnrecoverableError(const leveldb::Status& status) {
  leveldb_env::MethodID method;
  int error = 0;
  leveldb_env::ErrorParsingResult result = leveldb_env::ParseMethodAndError(
      status.ToString().c_str(), &method, &error);
  switch (result) {
    case leveldb_env::METHOD_ONLY:
    case leveldb_env::NONE:
      return false;
    case leveldb_env::METHOD_AND_PFE:
      switch (static_cast<base::PlatformFileError>(error)) {
        case base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED:
        case base::PLATFORM_FILE_ERROR_NO_MEMORY:
        case base::PLATFORM_FILE_ERROR_NO_SPACE:
          return true;
        default:
          return false;
      }
    case leveldb_env::METHOD_AND_ERRNO:
      switch (error) {
        case EMFILE:
        case ENOMEM:
        case ENOSPC:
          return true;
        default:
          return false;
      }
  }
  NOTREACHED();
  return false;
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

bool ResourceMetadataStorage::Iterator::GetCacheEntry(
    FileCacheEntry* cache_entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());

  // Try to seek to the cache entry.
  std::string current_key = it_->key().ToString();
  std::string cache_entry_key = GetCacheEntryKey(current_key);
  it_->Seek(leveldb::Slice(cache_entry_key));

  bool success = it_->Valid() &&
      it_->key().compare(cache_entry_key) == 0 &&
      cache_entry->ParseFromArray(it_->value().data(), it_->value().size());

  // Seek back to the original position.
  it_->Seek(leveldb::Slice(current_key));
  DCHECK(!IsAtEnd());
  DCHECK_EQ(current_key, it_->key().ToString());

  return success;
}

void ResourceMetadataStorage::Iterator::Advance() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());

  for (it_->Next() ; it_->Valid(); it_->Next()) {
    if (!IsChildEntryKey(it_->key()) &&
        !IsCacheEntryKey(it_->key()) &&
        entry_.ParseFromArray(it_->value().data(), it_->value().size()))
      break;
  }
}

bool ResourceMetadataStorage::Iterator::HasError() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->status().ok();
}

ResourceMetadataStorage::CacheEntryIterator::CacheEntryIterator(
    scoped_ptr<leveldb::Iterator> it) : it_(it.Pass()) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(it_);

  it_->SeekToFirst();
  AdvanceInternal();
}

ResourceMetadataStorage::CacheEntryIterator::~CacheEntryIterator() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool ResourceMetadataStorage::CacheEntryIterator::IsAtEnd() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->Valid();
}

const std::string& ResourceMetadataStorage::CacheEntryIterator::GetID() const {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());
  return id_;
}

const FileCacheEntry&
ResourceMetadataStorage::CacheEntryIterator::GetValue() const {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());
  return entry_;
}

void ResourceMetadataStorage::CacheEntryIterator::Advance() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());

  it_->Next();
  AdvanceInternal();
}

bool ResourceMetadataStorage::CacheEntryIterator::HasError() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->status().ok();
}

void ResourceMetadataStorage::CacheEntryIterator::AdvanceInternal() {
  for (; it_->Valid(); it_->Next()) {
    // Skip unparsable broken entries.
    // TODO(hashimoto): Broken entries should be cleaned up at some point.
    if (IsCacheEntryKey(it_->key()) &&
        entry_.ParseFromArray(it_->value().data(), it_->value().size())) {
      // Drop the suffix |kDBKeyDelimeter + kCacheEntryKeySuffix| from the key.
      const size_t kSuffixLength = arraysize(kCacheEntryKeySuffix) - 1;
      const int id_length = it_->key().size() - 1 - kSuffixLength;
      id_.assign(it_->key().data(), id_length);
      break;
    }
  }
}

ResourceMetadataStorage::ResourceMetadataStorage(
    const base::FilePath& directory_path,
    base::SequencedTaskRunner* blocking_task_runner)
    : directory_path_(directory_path),
      opened_existing_db_(false),
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
    ResourceMetadataHeader header;
    if (!GetHeader(&header) || header.version() != kDBVersion) {
      open_existing_result = DB_INIT_INCOMPATIBLE;
      LOG(INFO) << "Reject incompatible DB.";
    } else if (!CheckValidity()) {
      open_existing_result = DB_INIT_BROKEN;
      LOG(ERROR) << "Reject invalid DB.";
    }

    if (open_existing_result == DB_INIT_SUCCESS)
      opened_existing_db_ = true;
    else
      resource_map_.reset();
  }

  UMA_HISTOGRAM_ENUMERATION("Drive.MetadataDBOpenExistingResult",
                            open_existing_result,
                            DB_INIT_MAX_VALUE);

  if (IsUnrecoverableError(status))
    return false;

  DBInitStatus init_result = DB_INIT_SUCCESS;

  // Failed to open the existing DB, create new DB.
  if (!resource_map_) {
    // Clean up the destination.
    const bool kRecursive = true;
    base::DeleteFile(resource_map_path, kRecursive);

    // Create DB.
    options.max_open_files = 0;  // Use minimum.
    options.create_if_missing = true;

    status = leveldb::DB::Open(options, resource_map_path.AsUTF8Unsafe(), &db);
    if (status.ok()) {
      resource_map_.reset(db);

      // Set up header.
      ResourceMetadataHeader header;
      header.set_version(kDBVersion);
      if (!PutHeader(header)) {
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

bool ResourceMetadataStorage::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();

  ResourceMetadataHeader header;
  if (!GetHeader(&header)) {
    DLOG(ERROR) << "Failed to get the header.";
    return false;
  }
  header.set_largest_changestamp(largest_changestamp);
  return PutHeader(header);
}

int64 ResourceMetadataStorage::GetLargestChangestamp() {
  base::ThreadRestrictions::AssertIOAllowed();
  ResourceMetadataHeader header;
  if (!GetHeader(&header)) {
    DLOG(ERROR) << "Failed to get the header.";
    return 0;
  }
  return header.largest_changestamp();
}

bool ResourceMetadataStorage::PutEntry(const std::string& id,
                                       const ResourceEntry& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  std::string serialized_entry;
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry: " << id;
    return false;
  }

  leveldb::WriteBatch batch;

  // Remove from the old parent.
  ResourceEntry old_entry;
  if (GetEntry(id, &old_entry) && !old_entry.parent_local_id().empty()) {
    batch.Delete(GetChildEntryKey(old_entry.parent_local_id(),
                                  old_entry.base_name()));
  }

  // Add to the new parent.
  if (!entry.parent_local_id().empty())
    batch.Put(GetChildEntryKey(entry.parent_local_id(), entry.base_name()), id);

  // Put the entry itself.
  batch.Put(id, serialized_entry);

  const leveldb::Status status = resource_map_->Write(leveldb::WriteOptions(),
                                                      &batch);
  return status.ok();
}

bool ResourceMetadataStorage::GetEntry(const std::string& id,
                                       ResourceEntry* out_entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                                    leveldb::Slice(id),
                                                    &serialized_entry);
  return status.ok() && out_entry->ParseFromString(serialized_entry);
}

bool ResourceMetadataStorage::RemoveEntry(const std::string& id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  ResourceEntry entry;
  if (!GetEntry(id, &entry))
    return false;

  leveldb::WriteBatch batch;

  // Remove from the parent.
  if (!entry.parent_local_id().empty())
    batch.Delete(GetChildEntryKey(entry.parent_local_id(), entry.base_name()));

  // Remove the entry itself.
  batch.Delete(id);

  const leveldb::Status status = resource_map_->Write(leveldb::WriteOptions(),
                                                      &batch);
  return status.ok();
}

scoped_ptr<ResourceMetadataStorage::Iterator>
ResourceMetadataStorage::GetIterator() {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  return make_scoped_ptr(new Iterator(it.Pass()));
}

std::string ResourceMetadataStorage::GetChild(const std::string& parent_id,
                                              const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string child_id;
  resource_map_->Get(leveldb::ReadOptions(),
                     leveldb::Slice(GetChildEntryKey(parent_id, child_name)),
                     &child_id);
  return child_id;
}

void ResourceMetadataStorage::GetChildren(const std::string& parent_id,
                                          std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Iterate over all entries with keys starting with |parent_id|.
  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  for (it->Seek(parent_id);
       it->Valid() && it->key().starts_with(leveldb::Slice(parent_id));
       it->Next()) {
    if (IsChildEntryKey(it->key()))
      children->push_back(it->value().ToString());
  }
  DCHECK(it->status().ok());
}

bool ResourceMetadataStorage::PutCacheEntry(const std::string& id,
                                            const FileCacheEntry& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  std::string serialized_entry;
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry.";
    return false;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetCacheEntryKey(id)),
      leveldb::Slice(serialized_entry));
  return status.ok();
}

bool ResourceMetadataStorage::GetCacheEntry(const std::string& id,
                                            FileCacheEntry* out_entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetCacheEntryKey(id)),
      &serialized_entry);
  return status.ok() && out_entry->ParseFromString(serialized_entry);
}

bool ResourceMetadataStorage::RemoveCacheEntry(const std::string& id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!id.empty());

  const leveldb::Status status = resource_map_->Delete(
      leveldb::WriteOptions(),
      leveldb::Slice(GetCacheEntryKey(id)));
  return status.ok();
}

scoped_ptr<ResourceMetadataStorage::CacheEntryIterator>
ResourceMetadataStorage::GetCacheEntryIterator() {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  return make_scoped_ptr(new CacheEntryIterator(it.Pass()));
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
  std::string key = parent_id;
  key.push_back(kDBKeyDelimeter);
  key.append(child_name);
  key.push_back(kDBKeyDelimeter);
  return key;
}

bool ResourceMetadataStorage::PutHeader(
    const ResourceMetadataHeader& header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  if (!header.SerializeToString(&serialized_header)) {
    DLOG(ERROR) << "Failed to serialize the header";
    return false;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      leveldb::Slice(serialized_header));
  return status.ok();
}

bool ResourceMetadataStorage::GetHeader(ResourceMetadataHeader* header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      &serialized_header);
  return status.ok() && header->ParseFromString(serialized_header);
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
  // "|ID of A|"                    : ResourceEntry for entry A.
  // "|ID of A|\0CACHE"             : FileCacheEntry for entry A.
  // "|ID of A|\0|child name 1|\0"  : ID of the 1st child entry of entry A.
  // "|ID of A|\0|child name 2|\0"  : ID of the 2nd child entry of entry A.
  // ...
  // "|ID of A|\0|child name n|\0"  : ID of the nth child entry of entry A.
  // "|ID of B|"                    : ResourceEntry for entry B.
  // "|ID of B|\0CACHE"             : FileCacheEntry for entry B.
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
  std::string serialized_parent_entry;
  std::string child_id;
  for (it->Next(); it->Valid(); it->Next()) {
    // Count child entries.
    if (IsChildEntryKey(it->key())) {
      ++num_child_entries;
      continue;
    }

    // Ignore cache entries.
    if (IsCacheEntryKey(it->key()))
      continue;

    // Check if stored data is broken.
    if (!entry.ParseFromArray(it->value().data(), it->value().size())) {
      DLOG(ERROR) << "Broken entry detected";
      return false;
    }

    if (!entry.parent_local_id().empty()) {
      // Check if the parent entry is stored.
      leveldb::Status status = resource_map_->Get(
          options,
          leveldb::Slice(entry.parent_local_id()),
          &serialized_parent_entry);
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
