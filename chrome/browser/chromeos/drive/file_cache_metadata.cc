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

}  // namespace

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
    const base::FilePath& db_path) {
  AssertOnSequencedWorkerPool();

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

  if (!entry->ParseFromString(serialized)) {
    LOG(ERROR) << "Failed to parse " << serialized;
    return false;
  }
  return true;
}

scoped_ptr<FileCacheMetadata::Iterator> FileCacheMetadata::GetIterator() {
  AssertOnSequencedWorkerPool();

  scoped_ptr<leveldb::Iterator> iter(level_db_->NewIterator(
      leveldb::ReadOptions()));
  return make_scoped_ptr(new Iterator(iter.Pass()));
}

void FileCacheMetadata::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_.get() ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

}  // namespace internal
}  // namespace drive
