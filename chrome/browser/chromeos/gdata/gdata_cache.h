// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"

namespace gdata {

class GDataCache {
 public:
  // Enum defining GCache subdirectory location.
  // This indexes into |GDataFileSystem::cache_paths_| vector.
  enum CacheSubDirectoryType {
    CACHE_TYPE_META = 0,       // Downloaded feeds.
    CACHE_TYPE_PINNED,         // Symlinks to files in persistent dir that are
                               // pinned, or to /dev/null for non-existent
                               // files.
    CACHE_TYPE_OUTGOING,       // Symlinks to files in persistent or tmp dir to
                               // be uploaded.
    CACHE_TYPE_PERSISTENT,     // Files that are pinned or modified locally,
                               // not evictable, hopefully.
    CACHE_TYPE_TMP,            // Files that don't meet criteria to be in
                               // persistent dir, and hence evictable.
    CACHE_TYPE_TMP_DOWNLOADS,  // Downloaded files.
    CACHE_TYPE_TMP_DOCUMENTS,  // Temporary JSON files for hosted documents.
    NUM_CACHE_TYPES,           // This must be at the end.
  };

  // This is used as a bitmask for the cache state.
  enum CacheState {
    CACHE_STATE_NONE    = 0x0,
    CACHE_STATE_PINNED  = 0x1 << 0,
    CACHE_STATE_PRESENT = 0x1 << 1,
    CACHE_STATE_DIRTY   = 0x1 << 2,
    CACHE_STATE_MOUNTED = 0x1 << 3,
  };

  // Structure to store information of an existing cache file.
  struct CacheEntry {
    CacheEntry(const std::string& md5,
              CacheSubDirectoryType sub_dir_type,
              int cache_state)
    : md5(md5),
      sub_dir_type(sub_dir_type),
      cache_state(cache_state) {
    }

    bool IsPresent() const { return IsCachePresent(cache_state); }
    bool IsPinned() const { return IsCachePinned(cache_state); }
    bool IsDirty() const { return IsCacheDirty(cache_state); }
    bool IsMounted() const  { return IsCacheMounted(cache_state); }

    // For debugging purposes.
    std::string ToString() const;

    std::string md5;
    CacheSubDirectoryType sub_dir_type;
    int cache_state;
  };

  static bool IsCachePresent(int cache_state) {
    return cache_state & CACHE_STATE_PRESENT;
  }
  static bool IsCachePinned(int cache_state) {
    return cache_state & CACHE_STATE_PINNED;
  }
  static bool IsCacheDirty(int cache_state) {
    return cache_state & CACHE_STATE_DIRTY;
  }
  static bool IsCacheMounted(int cache_state) {
    return cache_state & CACHE_STATE_MOUNTED;
  }
  static int SetCachePresent(int cache_state) {
    return cache_state |= CACHE_STATE_PRESENT;
  }
  static int SetCachePinned(int cache_state) {
    return cache_state |= CACHE_STATE_PINNED;
  }
  static int SetCacheDirty(int cache_state) {
    return cache_state |= CACHE_STATE_DIRTY;
  }
  static int SetCacheMounted(int cache_state) {
    return cache_state |= CACHE_STATE_MOUNTED;
  }
  static int ClearCachePresent(int cache_state) {
    return cache_state &= ~CACHE_STATE_PRESENT;
  }
  static int ClearCachePinned(int cache_state) {
    return cache_state &= ~CACHE_STATE_PINNED;
  }
  static int ClearCacheDirty(int cache_state) {
    return cache_state &= ~CACHE_STATE_DIRTY;
  }
  static int ClearCacheMounted(int cache_state) {
    return cache_state &= ~CACHE_STATE_MOUNTED;
  }

  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, CacheEntry> CacheMap;

  virtual ~GDataCache();

  // Sets |cache_map_| data member to formal parameter |new_cache_map|.
  virtual void SetCacheMap(const CacheMap& new_cache_map) = 0;

  // Updates cache map with entry corresponding to |resource_id|.
  // Creates new entry if it doesn't exist, otherwise update the entry.
  virtual void UpdateCache(const std::string& resource_id,
                              const std::string& md5,
                              CacheSubDirectoryType subdir,
                              int cache_state) = 0;

  // Removes entry corresponding to |resource_id| from cache map.
  virtual void RemoveFromCache(const std::string& resource_id) = 0;

  // Returns the cache entry for file corresponding to |resource_id| and |md5|
  // if entry exists in cache map.  Otherwise, returns NULL.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  virtual scoped_ptr<CacheEntry> GetCacheEntry(const std::string& resource_id,
                                               const std::string& md5) = 0;

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  virtual void RemoveTemporaryFiles() = 0;

  // Factory methods for GDataCache.
  static scoped_ptr<GDataCache> CreateGDataCache();

 protected:
  GDataCache();

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataCache);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
