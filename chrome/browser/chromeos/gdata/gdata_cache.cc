// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache.h"

#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace gdata {
namespace {

std::string CacheSubDirectoryTypeToString(
    GDataCache::CacheSubDirectoryType subdir) {
  switch (subdir) {
    case GDataCache::CACHE_TYPE_META:
      return "meta";
    case GDataCache::CACHE_TYPE_PINNED:
      return "pinned";
    case GDataCache::CACHE_TYPE_OUTGOING:
      return "outgoing";
    case GDataCache::CACHE_TYPE_PERSISTENT:
      return "persistent";
    case GDataCache::CACHE_TYPE_TMP:
      return "tmp";
    case GDataCache::CACHE_TYPE_TMP_DOWNLOADS:
      return "tmp_downloads";
    case GDataCache::CACHE_TYPE_TMP_DOCUMENTS:
      return "tmp_documents";
    case GDataCache::NUM_CACHE_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return "unknown subdir";
}

}  // namespace

std::string GDataCache::CacheEntry::ToString() const {
  std::vector<std::string> cache_states;
  if (GDataCache::IsCachePresent(cache_state))
    cache_states.push_back("present");
  if (GDataCache::IsCachePinned(cache_state))
    cache_states.push_back("pinned");
  if (GDataCache::IsCacheDirty(cache_state))
    cache_states.push_back("dirty");

  return base::StringPrintf("md5=%s, subdir=%s, cache_state=%s",
                            md5.c_str(),
                            CacheSubDirectoryTypeToString(sub_dir_type).c_str(),
                            JoinString(cache_states, ',').c_str());
}

GDataCache::GDataCache() {
}

GDataCache::~GDataCache() {
}

class GDataCacheMap : public GDataCache {
 public:
  GDataCacheMap();

 protected:
  virtual ~GDataCacheMap();

 private:
  // GDataCache implementation.
  virtual void SetCacheMap(const CacheMap& new_cache_map) OVERRIDE;
  virtual void UpdateCache(const std::string& resource_id,
                              const std::string& md5,
                              CacheSubDirectoryType subdir,
                              int cache_state) OVERRIDE;
  virtual void RemoveFromCache(const std::string& resource_id) OVERRIDE;
  virtual scoped_ptr<CacheEntry> GetCacheEntry(const std::string& resource_id,
                                               const std::string& md5) OVERRIDE;
  virtual void RemoveTemporaryFiles() OVERRIDE;

  CacheMap cache_map_;
};

GDataCacheMap::GDataCacheMap() {
}

GDataCacheMap::~GDataCacheMap() {
  base::ThreadRestrictions::AssertIOAllowed();
  cache_map_.clear();
}

void GDataCacheMap::SetCacheMap(const CacheMap& new_cache_map)  {
  base::ThreadRestrictions::AssertIOAllowed();
  cache_map_ = new_cache_map;
}

void GDataCacheMap::UpdateCache(const std::string& resource_id,
                                const std::string& md5,
                                CacheSubDirectoryType subdir,
                                int cache_state) {
  base::ThreadRestrictions::AssertIOAllowed();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {  // New resource, create new entry.
    // Makes no sense to create new entry if cache state is NONE.
    DCHECK(cache_state != CACHE_STATE_NONE);
    if (cache_state != CACHE_STATE_NONE) {
      CacheEntry cache_entry(md5, subdir, cache_state);
      cache_map_.insert(std::make_pair(resource_id, cache_entry));
      DVLOG(1) << "Added res_id=" << resource_id
               << ", " << cache_entry.ToString();
    }
  } else {  // Resource exists.
    // If cache state is NONE, delete entry from cache map.
    if (cache_state == CACHE_STATE_NONE) {
      DVLOG(1) << "Deleting res_id=" << resource_id
               << ", " << iter->second.ToString();
      cache_map_.erase(iter);
    } else {  // Otherwise, update entry in cache map.
      iter->second.md5 = md5;
      iter->second.sub_dir_type = subdir;
      iter->second.cache_state = cache_state;
      DVLOG(1) << "Updated res_id=" << resource_id
               << ", " << iter->second.ToString();
    }
  }
}

void GDataCacheMap::RemoveFromCache(const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter != cache_map_.end()) {
    // Delete the CacheEntry and remove it from the map.
    cache_map_.erase(iter);
  }
}

scoped_ptr<GDataCache::CacheEntry> GDataCacheMap::GetCacheEntry(
    const std::string& resource_id,
    const std::string& md5) {
  base::ThreadRestrictions::AssertIOAllowed();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {
    DVLOG(1) << "Can't find " << resource_id << " in cache map";
    return scoped_ptr<CacheEntry>();
  }

  scoped_ptr<CacheEntry> cache_entry(new CacheEntry(iter->second));

  // If entry is not dirty, it's only valid if matches with non-empty |md5|.
  // If entry is dirty, its md5 may have been replaced by "local" during cache
  // initialization, so we don't compare md5.
  if (!cache_entry->IsDirty() && !md5.empty() && cache_entry->md5 != md5) {
    DVLOG(1) << "Non-matching md5: want=" << md5
             << ", found=[res_id=" << resource_id
             << ", " << cache_entry->ToString()
             << "]";
    return scoped_ptr<CacheEntry>();
  }

  DVLOG(1) << "Found entry for res_id=" << resource_id
           << ", " << cache_entry->ToString();

  return cache_entry.Pass();
}

void GDataCacheMap::RemoveTemporaryFiles() {
  base::ThreadRestrictions::AssertIOAllowed();

  CacheMap::iterator iter = cache_map_.begin();
  while (iter != cache_map_.end()) {
    if (iter->second.sub_dir_type == CACHE_TYPE_TMP) {
      // Post-increment the iterator to avoid iterator invalidation.
      cache_map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

// static
scoped_ptr<GDataCache> GDataCache::CreateGDataCache() {
  return scoped_ptr<GDataCache>(new GDataCacheMap());
}

}  // namespace gdata
