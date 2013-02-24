// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_METADATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace drive {

class DriveCacheEntry;

// Callback for Iterate().
typedef base::Callback<void(const std::string& resource_id,
                            const DriveCacheEntry& cache_entry)>
    CacheIterateCallback;

// DriveCacheMetadata is interface to maintain metadata of DriveCache's cached
// files. This class only manages metadata. File operations are done by
// DriveCache.
// All member access including ctor and dtor must be made on the blocking pool.
class DriveCacheMetadata {
 public:
  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, DriveCacheEntry> CacheMap;

  // Database path.
  static const base::FilePath::CharType* kDriveCacheMetadataDBPath;

  virtual ~DriveCacheMetadata();

  // Creates DriveCacheMetadata instance.
  static scoped_ptr<DriveCacheMetadata> CreateDriveCacheMetadata(
      base::SequencedTaskRunner* blocking_task_runner);

  // Creates DriveCacheMetadata instance. This uses FakeDriveCacheMetadata,
  // which is an in-memory implementation and faster than DriveCacheMetadataDB.
  static scoped_ptr<DriveCacheMetadata> CreateDriveCacheMetadataForTesting(
      base::SequencedTaskRunner* blocking_task_runner);

  // Initialize the cache metadata store. Returns true on success.
  virtual bool Initialize(const std::vector<base::FilePath>& cache_paths) = 0;
  // Adds a new cache entry corresponding to |resource_id| if it doesn't
  // exist, otherwise update the existing entry.
  virtual void AddOrUpdateCacheEntry(const std::string& resource_id,
                                     const DriveCacheEntry& cache_entry) = 0;

  // Removes entry corresponding to |resource_id| from cache map.
  virtual void RemoveCacheEntry(const std::string& resource_id) = 0;

  // Gets the cache entry for file corresponding to |resource_id| and |md5|
  // and returns true if entry exists in cache map.  Otherwise, returns false.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  virtual bool GetCacheEntry(const std::string& resource_id,
                             const std::string& md5,
                             DriveCacheEntry* entry) = 0;

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  virtual void RemoveTemporaryFiles() = 0;

  // Iterates over all the cache entries synchronously. |callback| is called
  // on each cache entry.
  virtual void Iterate(const CacheIterateCallback& callback) = 0;

 protected:
  explicit DriveCacheMetadata(base::SequencedTaskRunner* blocking_task_runner);

  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

 private:
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DriveCacheMetadata);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_METADATA_H_
