// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache_metadata.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace drive {
namespace internal {

namespace {

typedef std::map<std::string, FileCacheEntry> CacheMap;

enum DBOpenStatus {
  DB_OPEN_SUCCESS,
  DB_OPEN_FAILURE_CORRUPTION,
  DB_OPEN_FAILURE_OTHER,
  DB_OPEN_FAILURE_UNRECOVERABLE,
  DB_OPEN_MAX_VALUE,
};

// A map table of resource ID to file path.
typedef std::map<std::string, base::FilePath> ResourceIdToFilePathMap;

// Scans cache subdirectory and build or update |cache_map| with found files.
//
// The resource IDs and file paths of discovered files are collected as a
// ResourceIdToFilePathMap, if these are processed properly.
void ScanCacheDirectory(const std::vector<base::FilePath>& cache_paths,
                        FileCache::CacheSubDirectoryType sub_dir_type,
                        CacheMap* cache_map,
                        ResourceIdToFilePathMap* processed_file_map) {
  DCHECK(cache_map);
  DCHECK(processed_file_map);

  file_util::FileEnumerator enumerator(cache_paths[sub_dir_type],
                                       false,  // not recursive
                                       file_util::FileEnumerator::FILES,
                                       util::kWildCard);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Extract resource_id and md5 from filename.
    std::string resource_id;
    std::string md5;
    std::string extra_extension;
    util::ParseCacheFilePath(current, &resource_id, &md5, &extra_extension);

    // Determine cache state.
    FileCacheEntry cache_entry;
    cache_entry.set_md5(md5);
    if (sub_dir_type == FileCache::CACHE_TYPE_PERSISTENT)
      cache_entry.set_is_persistent(true);

    if (extra_extension == util::kMountedArchiveFileExtension) {
      // Mounted archives in cache should be unmounted upon logout/shutdown.
      // But if we encounter a mounted file at start, delete it and create an
      // entry with not PRESENT state.
      DCHECK(sub_dir_type == FileCache::CACHE_TYPE_PERSISTENT);
      file_util::Delete(current, false);
    } else {
      // The cache file is present.
      cache_entry.set_is_present(true);

      // Adds the dirty bit if |md5| indicates that the file is dirty, and
      // the file is in the persistent directory.
      if (md5 == util::kLocallyModifiedFileExtension) {
        if (sub_dir_type == FileCache::CACHE_TYPE_PERSISTENT) {
          cache_entry.set_is_dirty(true);
        } else {
          LOG(WARNING) << "Removing a dirty file in tmp directory: "
                       << current.value();
          file_util::Delete(current, false);
          continue;
        }
      }
    }

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(resource_id, cache_entry));
    processed_file_map->insert(std::make_pair(resource_id, current));
  }
}

void ScanCachePaths(const std::vector<base::FilePath>& cache_paths,
                    CacheMap* cache_map) {
  DVLOG(1) << "Scanning directories";

  // Scan cache persistent and tmp directories to enumerate all files and create
  // corresponding entries for cache map.
  ResourceIdToFilePathMap persistent_file_map;
  ScanCacheDirectory(cache_paths,
                     FileCache::CACHE_TYPE_PERSISTENT,
                     cache_map,
                     &persistent_file_map);
  ResourceIdToFilePathMap tmp_file_map;
  ScanCacheDirectory(cache_paths,
                     FileCache::CACHE_TYPE_TMP,
                     cache_map,
                     &tmp_file_map);

  // On DB corruption, keep only dirty-and-committed files in persistent
  // directory. Other files are deleted or moved to temporary directory.
  for (ResourceIdToFilePathMap::const_iterator iter =
           persistent_file_map.begin();
       iter != persistent_file_map.end(); ++iter) {
    const std::string& resource_id = iter->first;
    const base::FilePath& file_path = iter->second;

    CacheMap::iterator cache_map_iter = cache_map->find(resource_id);
    if (cache_map_iter != cache_map->end()) {
      FileCacheEntry* cache_entry = &cache_map_iter->second;
      const bool is_dirty = cache_entry->is_dirty();
      if (!is_dirty) {
        // If the file is not dirty, move to temporary directory.
        base::FilePath new_file_path =
            cache_paths[FileCache::CACHE_TYPE_TMP].Append(
                file_path.BaseName());
        DLOG(WARNING) << "Moving: " << file_path.value()
                      << " to: " << new_file_path.value();
        file_util::Move(file_path, new_file_path);
        cache_entry->set_is_persistent(false);
      }
    }
  }
  DVLOG(1) << "Directory scan finished";
}

// Returns true if |md5| matches the one in |cache_entry| with some
// exceptions. See the function definition for details.
bool CheckIfMd5Matches(const std::string& md5,
                       const FileCacheEntry& cache_entry) {
  if (cache_entry.is_dirty()) {
    // If the entry is dirty, its MD5 may have been replaced by "local"
    // during cache initialization, so we don't compare MD5.
    return true;
  } else if (cache_entry.is_pinned() && cache_entry.md5().empty()) {
    // If the entry is pinned, it's ok for the entry to have an empty
    // MD5. This can happen if the pinned file is not fetched. MD5 for pinned
    // files are collected from files in "persistent" directory, but the
    // persistent files do not exist if these are not fetched yet.
    return true;
  } else if (md5.empty()) {
    // If the MD5 matching is not requested, don't check MD5.
    return true;
  } else if (md5 == cache_entry.md5()) {
    // Otherwise, compare the MD5.
    return true;
  }
  return false;
}

}  // namespace

// static
const base::FilePath::CharType* FileCacheMetadata::kCacheMetadataDBPath =
    FILE_PATH_LITERAL("cache_metadata.db");

FileCacheMetadata::Iterator::Iterator(scoped_ptr<leveldb::Iterator> it)
    : it_(it.Pass()) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(it_);

  it_->SeekToFirst();
  AdvanceInternal();
}

FileCacheMetadata::Iterator::~Iterator() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool FileCacheMetadata::Iterator::IsAtEnd() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->Valid();
}

std::string FileCacheMetadata::Iterator::GetKey() const {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());
  return it_->key().ToString();
}

const FileCacheEntry& FileCacheMetadata::Iterator::GetValue() const {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());
  return entry_;
}

void FileCacheMetadata::Iterator::Advance() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsAtEnd());

  it_->Next();
  AdvanceInternal();
}

bool FileCacheMetadata::Iterator::HasError() const {
  base::ThreadRestrictions::AssertIOAllowed();
  return !it_->status().ok();
}

void FileCacheMetadata::Iterator::AdvanceInternal() {
  for (; it_->Valid(); it_->Next()) {
    // Skip unparsable broken entries.
    // TODO(hashimoto): Broken entries should be cleaned up at some point.
    if (entry_.ParseFromArray(it_->value().data(), it_->value().size()))
      break;
  }
}

FileCacheMetadata::FileCacheMetadata(
    base::SequencedTaskRunner* blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {
  AssertOnSequencedWorkerPool();
}

FileCacheMetadata::~FileCacheMetadata() {
  AssertOnSequencedWorkerPool();
}

bool FileCacheMetadata::Initialize(
    const std::vector<base::FilePath>& cache_paths) {
  AssertOnSequencedWorkerPool();

  const base::FilePath db_path =
      cache_paths[FileCache::CACHE_TYPE_META].Append(kCacheMetadataDBPath);
  DVLOG(1) << "db path=" << db_path.value();

  bool scan_cache = !file_util::PathExists(db_path);

  leveldb::DB* level_db = NULL;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status db_status = leveldb::DB::Open(options, db_path.AsUTF8Unsafe(),
                                                &level_db);

  // Delete the db and scan the physical cache. This will fix a corrupt db, but
  // perhaps not other causes of failed DB::Open.
  DBOpenStatus uma_status = DB_OPEN_SUCCESS;
  if (!db_status.ok()) {
    LOG(WARNING) << "Cache db failed to open: " << db_status.ToString();
    uma_status = db_status.IsCorruption() ?
        DB_OPEN_FAILURE_CORRUPTION : DB_OPEN_FAILURE_OTHER;
    const bool deleted = file_util::Delete(db_path, true);
    DCHECK(deleted);
    db_status = leveldb::DB::Open(options, db_path.value(), &level_db);
    if (!db_status.ok()) {
      LOG(WARNING) << "Still failed to open: " << db_status.ToString();
      UMA_HISTOGRAM_ENUMERATION("Drive.CacheDBOpenStatus",
                                DB_OPEN_FAILURE_UNRECOVERABLE,
                                DB_OPEN_MAX_VALUE);
      // Failed to open the cache metadata DB. Drive will be disabled.
      return false;
    }

    scan_cache = true;
  }
  UMA_HISTOGRAM_ENUMERATION("Drive.CacheDBOpenStatus", uma_status,
                            DB_OPEN_MAX_VALUE);
  DCHECK(level_db);
  level_db_.reset(level_db);

  // We scan the cache directories to initialize the cache database if we
  // were previously using the cache map.
  if (scan_cache) {
    CacheMap cache_map;
    ScanCachePaths(cache_paths, &cache_map);
    for (CacheMap::const_iterator it = cache_map.begin();
         it != cache_map.end(); ++it) {
      AddOrUpdateCacheEntry(it->first, it->second);
    }
  }

  return true;
}

void FileCacheMetadata::AddOrUpdateCacheEntry(
    const std::string& resource_id,
    const FileCacheEntry& cache_entry) {
  AssertOnSequencedWorkerPool();

  DVLOG(1) << "AddOrUpdateCacheEntry, resource_id=" << resource_id;
  std::string serialized;
  const bool ok = cache_entry.SerializeToString(&serialized);
  if (ok)
    level_db_->Put(leveldb::WriteOptions(),
                   leveldb::Slice(resource_id),
                   leveldb::Slice(serialized));
}

void FileCacheMetadata::RemoveCacheEntry(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  DVLOG(1) << "RemoveCacheEntry, resource_id=" << resource_id;
  level_db_->Delete(leveldb::WriteOptions(), leveldb::Slice(resource_id));
}

bool FileCacheMetadata::GetCacheEntry(const std::string& resource_id,
                                      const std::string& md5,
                                      FileCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();

  std::string serialized;
  const leveldb::Status status = level_db_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(resource_id), &serialized);
  if (!status.ok()) {
    DVLOG(1) << "Can't find " << resource_id << " in cache db";
    return false;
  }

  FileCacheEntry cache_entry;
  const bool ok = cache_entry.ParseFromString(serialized);
  if (!ok) {
    LOG(ERROR) << "Failed to parse " << serialized;
    return false;
  }

  if (!CheckIfMd5Matches(md5, cache_entry)) {
    return false;
  }

  *entry = cache_entry;
  return true;
}

void FileCacheMetadata::RemoveTemporaryFiles() {
  AssertOnSequencedWorkerPool();

  scoped_ptr<leveldb::Iterator> iter(level_db_->NewIterator(
      leveldb::ReadOptions()));
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    FileCacheEntry cache_entry;
    const bool ok = cache_entry.ParseFromArray(iter->value().data(),
                                               iter->value().size());
    if (ok && !cache_entry.is_persistent())
      level_db_->Delete(leveldb::WriteOptions(), iter->key());
  }
}

scoped_ptr<FileCacheMetadata::Iterator> FileCacheMetadata::GetIterator() {
  AssertOnSequencedWorkerPool();

  scoped_ptr<leveldb::Iterator> iter(level_db_->NewIterator(
      leveldb::ReadOptions()));
  return make_scoped_ptr(new Iterator(iter.Pass()));
}

void FileCacheMetadata::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_ ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

}  // namespace internal
}  // namespace drive
