// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_
#pragma once

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
                              const GDataCache::CacheEntry& cache_entry)>
      IterateCallback;

  // |pool| and |sequence_token| are used to assert that the functions are
  // called on the right sequenced worker pool with the right sequence token.
  //
  // For testing, the thread assertion can be disabled by passing NULL and
  // the default value of SequenceToken.
  GDataCacheMetadata(
      base::SequencedWorkerPool* pool,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);
  virtual ~GDataCacheMetadata();

  // Updates cache map with entry corresponding to |resource_id|.
  // Creates new entry if it doesn't exist, otherwise update the entry.
  virtual void UpdateCache(const std::string& resource_id,
                           const std::string& md5,
                           GDataCache::CacheSubDirectoryType subdir,
                           int cache_state) = 0;

  // Removes entry corresponding to |resource_id| from cache map.
  virtual void RemoveFromCache(const std::string& resource_id) = 0;

  // Returns the cache entry for file corresponding to |resource_id| and |md5|
  // if entry exists in cache map.  Otherwise, returns NULL.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  virtual scoped_ptr<GDataCache::CacheEntry> GetCacheEntry(
      const std::string& resource_id,
      const std::string& md5) = 0;

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  virtual void RemoveTemporaryFiles() = 0;

  // Iterates over all the cache entries synchronously. |callback| is called
  // on each cache entry.
  virtual void Iterate(const IterateCallback& callback) = 0;

 protected:
  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

 private:
  base::SequencedWorkerPool* pool_;
  const base::SequencedWorkerPool::SequenceToken& sequence_token_;

  DISALLOW_COPY_AND_ASSIGN(GDataCacheMetadata);
};

// GDataCacheMetadata implementation with std::map;
class GDataCacheMetadataMap : public GDataCacheMetadata {
 public:
  GDataCacheMetadataMap(
      base::SequencedWorkerPool* pool,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);
  virtual ~GDataCacheMetadataMap();

  // Initializes the data.
  void Initialize(const std::vector<FilePath>& cache_paths);

  // GDataCacheMetadata overrides:
  virtual void UpdateCache(const std::string& resource_id,
                           const std::string& md5,
                           GDataCache::CacheSubDirectoryType subdir,
                           int cache_state) OVERRIDE;
  virtual void RemoveFromCache(const std::string& resource_id) OVERRIDE;
  virtual scoped_ptr<GDataCache::CacheEntry> GetCacheEntry(
      const std::string& resource_id,
      const std::string& md5) OVERRIDE;
  virtual void RemoveTemporaryFiles() OVERRIDE;
  virtual void Iterate(const IterateCallback& callback) OVERRIDE;

 private:
  friend class GDataCacheMetadataMapTest;
  FRIEND_TEST_ALL_PREFIXES(GDataCacheMetadataMapTest, RemoveTemporaryFilesTest);

   // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, GDataCache::CacheEntry> CacheMap;

  // Scans cache subdirectory and build or update |cache_map|
  // with found file blobs or symlinks.
  void ScanCacheDirectory(const std::vector<FilePath>& cache_paths,
                          GDataCache::CacheSubDirectoryType sub_dir_type,
                          CacheMap* cache_map);

  // Returns true if |md5| matches the one in |cache_entry| with some
  // exceptions. See the function definition for details.
  static bool CheckIfMd5Matches(const std::string& md5,
                                const GDataCache::CacheEntry& cache_entry);

  CacheMap cache_map_;

  DISALLOW_COPY_AND_ASSIGN(GDataCacheMetadataMap);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_METADATA_H_
