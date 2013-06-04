// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace leveldb {
class DB;
}  // namespace leveldb

namespace drive {

class FileCacheEntry;

// Callback for Iterate().
typedef base::Callback<void(const std::string& resource_id,
                            const FileCacheEntry& cache_entry)>
    CacheIterateCallback;

namespace internal {

// FileCacheMetadata maintains metadata of FileCache's cached files.
// This class only manages metadata. File operations are done by FileCache.
// All member access including ctor and dtor must be made on the blocking pool.
class FileCacheMetadata {
 public:
  // Database path.
  static const base::FilePath::CharType* kCacheMetadataDBPath;

  // Tests are allowed to pass NULL as |blocking_task_runner|.
  explicit FileCacheMetadata(base::SequencedTaskRunner* blocking_task_runner);

  ~FileCacheMetadata();

  // Initialize the cache metadata store. Returns true on success.
  bool Initialize(const std::vector<base::FilePath>& cache_paths);
  // Adds a new cache entry corresponding to |resource_id| if it doesn't
  // exist, otherwise update the existing entry.
  void AddOrUpdateCacheEntry(const std::string& resource_id,
                             const FileCacheEntry& cache_entry);

  // Removes entry corresponding to |resource_id| from cache map.
  void RemoveCacheEntry(const std::string& resource_id);

  // Gets the cache entry for file corresponding to |resource_id| and |md5|
  // and returns true if entry exists in cache map.  Otherwise, returns false.
  // |md5| can be empty if only matching |resource_id| is desired.
  bool GetCacheEntry(const std::string& resource_id,
                     const std::string& md5,
                     FileCacheEntry* entry);

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  void RemoveTemporaryFiles();

  // Iterates over all the cache entries synchronously. |callback| is called
  // on each cache entry.
  void Iterate(const CacheIterateCallback& callback);

 private:
  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<leveldb::DB> level_db_;

  DISALLOW_COPY_AND_ASSIGN(FileCacheMetadata);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_METADATA_H_
