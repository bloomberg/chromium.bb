// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_

#include <list>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "content/browser/cache_storage/cache_storage.h"

namespace content {

// CacheStorageIndex maintains an ordered list of metadata (CacheMetadata)
// for each cache owned by a CacheStorage object. This class is not thread safe,
// and is owned by the CacheStorage.
class CONTENT_EXPORT CacheStorageIndex {
 public:
  struct CacheMetadata {
    CacheMetadata(const std::string& name, int64_t size)
        : name(name), size(size) {}
    std::string name;
    // The size (in bytes) of the cache. Set to CacheStorage::kSizeUnknown if
    // size not known.
    int64_t size;
  };

  CacheStorageIndex();
  ~CacheStorageIndex();

  CacheStorageIndex& operator=(CacheStorageIndex&& rhs);

  void Insert(const CacheMetadata& cache_metadata);
  void Delete(const std::string& cache_name);

  // Sets the cache size. Returns true if the new size is different than the
  // current size else false.
  bool SetCacheSize(const std::string& cache_name, int64_t size);

  // Return the size (in bytes) of the specified cache. Will return
  // CacheStorage::kSizeUnknown if the specified cache does not exist.
  int64_t GetCacheSize(const std::string& cache_name) const;

  const std::list<CacheMetadata>& ordered_cache_metadata() const {
    return ordered_cache_metadata_;
  }

  size_t num_entries() const { return ordered_cache_metadata_.size(); }

  // Will calculate (if necessary), and return the total sum of all cache sizes.
  int64_t GetStorageSize();

  // Mark the cache as doomed. This removes the cache metadata from the index.
  // All const methods (eg: num_entries) will behave as if the doomed cache is
  // not present in the index. Prior to calling any non-const method the doomed
  // cache must either be finalized (by calling FinalizeDoomedCache) or restored
  // (by calling RestoreDoomedCache).
  //
  // RestoreDoomedCache restores the metadata to the index at the original
  // position prior to calling DoomCache.
  void DoomCache(const std::string& cache_name);
  void FinalizeDoomedCache();
  void RestoreDoomedCache();

 private:
  void UpdateStorageSize();
  void ClearDoomedCache();

  // Use a list to keep saved iterators valid during insert/erase.
  // Note: ordered by cache creation.
  std::list<CacheMetadata> ordered_cache_metadata_;
  std::unordered_map<std::string, std::list<CacheMetadata>::iterator>
      cache_metadata_map_;

  // The total size of all caches in this store.
  int64_t storage_size_ = CacheStorage::kSizeUnknown;

  // The doomed cache metadata saved when calling DoomCache.
  CacheMetadata doomed_cache_metadata_;
  std::list<CacheMetadata>::iterator after_doomed_cache_metadata_;
  bool has_doomed_cache_ = false;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageIndex);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_
