// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_

#include <map>
#include <vector>
#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"

namespace gdata {

// GDataCacheMetadata is interface to maintain metadata of GDataCache's cached
// files. This class only manages metadata. File operations are done by
// GDataCache.
// All member access including ctor and dtor must be made on the blocking pool.
class GDataCacheMetadata {
 public:
  // Callback for Iterate().
  typedef base::Callback<void(const std::string& resource_id,
                              const GDataCacheEntry& cache_entry)>
      IterateCallback;

  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, GDataCacheEntry> CacheMap;

  virtual ~GDataCacheMetadata();

  // |pool| and |sequence_token| are used to assert that the functions are
  // called on the right sequenced worker pool with the right sequence token.
  //
  // For testing, the thread assertion can be disabled by passing NULL and
  // the default value of SequenceToken.
  static scoped_ptr<GDataCacheMetadata> CreateGDataCacheMetadata(
      base::SequencedTaskRunner* blocking_task_runner);

  // Initialize the cache metadata store.
  virtual void Initialize(const std::vector<FilePath>& cache_paths) = 0;
  // Adds a new cache entry corresponding to |resource_id| if it doesn't
  // exist, otherwise update the existing entry.
  virtual void AddOrUpdateCacheEntry(const std::string& resource_id,
                                     const GDataCacheEntry& cache_entry) = 0;

  // Removes entry corresponding to |resource_id| from cache map.
  virtual void RemoveCacheEntry(const std::string& resource_id) = 0;

  // Gets the cache entry for file corresponding to |resource_id| and |md5|
  // and returns true if entry exists in cache map.  Otherwise, returns false.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  virtual bool GetCacheEntry(const std::string& resource_id,
                             const std::string& md5,
                             GDataCacheEntry* entry) = 0;

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  virtual void RemoveTemporaryFiles() = 0;

  // Iterates over all the cache entries synchronously. |callback| is called
  // on each cache entry.
  virtual void Iterate(const IterateCallback& callback) = 0;

  // Force a rescan of cache directories, for testing.
  virtual void ForceRescanForTesting(
      const std::vector<FilePath>& cache_paths) = 0;

 protected:
  explicit GDataCacheMetadata(base::SequencedTaskRunner* blocking_task_runner);

  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

 private:
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GDataCacheMetadata);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_
