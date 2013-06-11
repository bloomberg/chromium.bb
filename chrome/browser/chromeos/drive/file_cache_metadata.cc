// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache_metadata.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace drive {
namespace internal {

namespace {

enum DBOpenStatus {
  DB_OPEN_SUCCESS,
  DB_OPEN_FAILURE_CORRUPTION,
  DB_OPEN_FAILURE_OTHER,
  DB_OPEN_FAILURE_UNRECOVERABLE,
  DB_OPEN_MAX_VALUE,
};

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

FileCacheMetadata::InitializeResult FileCacheMetadata::Initialize(
    const base::FilePath& db_directory_path) {
  AssertOnSequencedWorkerPool();

  const base::FilePath db_path = db_directory_path.Append(kCacheMetadataDBPath);
  DVLOG(1) << "db path=" << db_path.value();

  bool created = !file_util::PathExists(db_path);

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
      return INITIALIZE_FAILED;
    }

    created = true;
  }
  UMA_HISTOGRAM_ENUMERATION("Drive.CacheDBOpenStatus", uma_status,
                            DB_OPEN_MAX_VALUE);
  DCHECK(level_db);
  level_db_.reset(level_db);

  return created ? INITIALIZE_CREATED : INITIALIZE_OPENED;
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
