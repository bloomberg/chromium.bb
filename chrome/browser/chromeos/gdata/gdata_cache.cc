// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache.h"

#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"

namespace gdata {
namespace {

const char kLocallyModifiedFileExtension[] = "local";

const FilePath::CharType kGDataCacheVersionDir[] = FILE_PATH_LITERAL("v1");
const FilePath::CharType kGDataCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const FilePath::CharType kGDataCachePinnedDir[] = FILE_PATH_LITERAL("pinned");
const FilePath::CharType kGDataCacheOutgoingDir[] =
    FILE_PATH_LITERAL("outgoing");
const FilePath::CharType kGDataCachePersistentDir[] =
    FILE_PATH_LITERAL("persistent");
const FilePath::CharType kGDataCacheTmpDir[] = FILE_PATH_LITERAL("tmp");
const FilePath::CharType kGDataCacheTmpDownloadsDir[] =
    FILE_PATH_LITERAL("tmp/downloads");
const FilePath::CharType kGDataCacheTmpDocumentsDir[] =
    FILE_PATH_LITERAL("tmp/documents");

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

// Returns file paths for all the cache sub directories under
// |cache_root_path|.
std::vector<FilePath> GetCachePaths(const FilePath& cache_root_path) {
  std::vector<FilePath> cache_paths;
  // The order should match GDataCache::CacheSubDirectoryType enum.
  cache_paths.push_back(cache_root_path.Append(kGDataCacheMetaDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCachePinnedDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheOutgoingDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCachePersistentDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDownloadsDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDocumentsDir));
  return cache_paths;
}

}  // namespace

const char GDataCache::kMountedArchiveFileExtension[] = "mounted";

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

GDataCache::GDataCache(
    const FilePath& cache_root_path,
    base::SequencedWorkerPool* pool,
    const base::SequencedWorkerPool::SequenceToken& sequence_token)
    : cache_root_path_(cache_root_path),
      cache_paths_(GetCachePaths(cache_root_path_)),
      pool_(pool),
      sequence_token_(sequence_token) {
}

GDataCache::~GDataCache() {
}

FilePath GDataCache::GetCacheDirectoryPath(
    CacheSubDirectoryType sub_dir_type) const {
  DCHECK_LE(0, sub_dir_type);
  DCHECK_GT(NUM_CACHE_TYPES, sub_dir_type);
  return cache_paths_[sub_dir_type];
}

FilePath GDataCache::GetCacheFilePath(const std::string& resource_id,
                                      const std::string& md5,
                                      CacheSubDirectoryType sub_dir_type,
                                      CachedFileOrigin file_origin) const {
  DCHECK(sub_dir_type != CACHE_TYPE_META);

  // Runs on any thread.
  // Filename is formatted as resource_id.md5, i.e. resource_id is the base
  // name and md5 is the extension.
  std::string base_name = util::EscapeCacheFileName(resource_id);
  if (file_origin == CACHED_FILE_LOCALLY_MODIFIED) {
    DCHECK(sub_dir_type == CACHE_TYPE_PERSISTENT);
    base_name += FilePath::kExtensionSeparator;
    base_name += kLocallyModifiedFileExtension;
  } else if (!md5.empty()) {
    base_name += FilePath::kExtensionSeparator;
    base_name += util::EscapeCacheFileName(md5);
  }
  // For mounted archives the filename is formatted as resource_id.md5.mounted,
  // i.e. resource_id.md5 is the base name and ".mounted" is the extension
  if (file_origin == CACHED_FILE_MOUNTED) {
     DCHECK(sub_dir_type == CACHE_TYPE_PERSISTENT);
     base_name += FilePath::kExtensionSeparator;
     base_name += kMountedArchiveFileExtension;
  }
  return GetCacheDirectoryPath(sub_dir_type).Append(base_name);
}

void GDataCache::AssertOnSequencedWorkerPool() {
  DCHECK(!pool_ || pool_->IsRunningSequenceOnCurrentThread(sequence_token_));
}

bool GDataCache::IsUnderGDataCacheDirectory(const FilePath& path) const {
  return cache_root_path_ == path || cache_root_path_.IsParent(path);
}

class GDataCacheMap : public GDataCache {
 public:
  GDataCacheMap(
      const FilePath& cache_root_path,
      base::SequencedWorkerPool* pool,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);

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

GDataCacheMap::GDataCacheMap(
    const FilePath& cache_root_path,
    base::SequencedWorkerPool* pool,
    const base::SequencedWorkerPool::SequenceToken& sequence_token)
    : GDataCache(cache_root_path, pool, sequence_token) {
}

GDataCacheMap::~GDataCacheMap() {
  // TODO(satorux): Enable this once all callers are fixed: crbug.com/131826.
  // AssertOnSequencedWorkerPool();
  cache_map_.clear();
}

void GDataCacheMap::SetCacheMap(const CacheMap& new_cache_map)  {
  AssertOnSequencedWorkerPool();
  cache_map_ = new_cache_map;
}

void GDataCacheMap::UpdateCache(const std::string& resource_id,
                                const std::string& md5,
                                CacheSubDirectoryType subdir,
                                int cache_state) {
  AssertOnSequencedWorkerPool();

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
  AssertOnSequencedWorkerPool();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter != cache_map_.end()) {
    // Delete the CacheEntry and remove it from the map.
    cache_map_.erase(iter);
  }
}

scoped_ptr<GDataCache::CacheEntry> GDataCacheMap::GetCacheEntry(
    const std::string& resource_id,
    const std::string& md5) {
  // TODO(satorux): Enable this once all callers are fixed: crbug.com/131826.
  // AssertOnSequencedWorkerPool();

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
  AssertOnSequencedWorkerPool();

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
scoped_ptr<GDataCache> GDataCache::CreateGDataCache(
    const FilePath& cache_root_path,
    base::SequencedWorkerPool* pool,
    const base::SequencedWorkerPool::SequenceToken& sequence_token) {
  return scoped_ptr<GDataCache>(new GDataCacheMap(
      cache_root_path, pool, sequence_token));
}

// static
FilePath GDataCache::GetCacheRootPath(Profile* profile) {
  FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  FilePath cache_root_path =
      cache_base_path.Append(chrome::kGDataCacheDirname);
  return cache_root_path.Append(kGDataCacheVersionDir);
}

}  // namespace gdata
