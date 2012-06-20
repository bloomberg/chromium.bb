// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache_metadata.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"

namespace gdata {

namespace {

// Creates cache directory and its sub-directories if they don't exist.
// TODO(glotov): take care of this when the setup and cleanup part is landed,
// noting that these directories need to be created for development in linux box
// and unittest. (http://crosbug.com/27577)
bool CreateCacheDirectories(const std::vector<FilePath>& paths_to_create) {
  bool success = true;

  for (size_t i = 0; i < paths_to_create.size(); ++i) {
    if (file_util::DirectoryExists(paths_to_create[i]))
      continue;

    if (!file_util::CreateDirectory(paths_to_create[i])) {
      // Error creating this directory, record error and proceed with next one.
      success = false;
      PLOG(ERROR) << "Error creating directory " << paths_to_create[i].value();
    } else {
      DVLOG(1) << "Created directory " << paths_to_create[i].value();
    }
  }
  return success;
}

// Changes the permissions of |file_path| to |permissions|.
bool ChangeFilePermissions(const FilePath& file_path, mode_t permissions) {
  if (HANDLE_EINTR(chmod(file_path.value().c_str(), permissions)) != 0) {
    PLOG(ERROR) << "Error changing permissions of " << file_path.value();
    return false;
  }
  DVLOG(1) << "Changed permissions of " << file_path.value();
  return true;
}

}  // namespace

GDataCacheMetadata::GDataCacheMetadata(
    base::SequencedWorkerPool* pool,
    const base::SequencedWorkerPool::SequenceToken& sequence_token)
    : pool_(pool),
      sequence_token_(sequence_token) {
  AssertOnSequencedWorkerPool();
}

GDataCacheMetadata::~GDataCacheMetadata() {
  AssertOnSequencedWorkerPool();
}

void GDataCacheMetadata::AssertOnSequencedWorkerPool() {
  DCHECK(!pool_ || pool_->IsRunningSequenceOnCurrentThread(sequence_token_));
}

GDataCacheMetadataMap::GDataCacheMetadataMap(
    base::SequencedWorkerPool* pool,
    const base::SequencedWorkerPool::SequenceToken& sequence_token)
    : GDataCacheMetadata(pool, sequence_token) {
  AssertOnSequencedWorkerPool();
}

GDataCacheMetadataMap::~GDataCacheMetadataMap() {
  AssertOnSequencedWorkerPool();
}


void GDataCacheMetadataMap::Initialize(
    const std::vector<FilePath>& cache_paths) {
  AssertOnSequencedWorkerPool();

  if (cache_paths.size() < GDataCache::NUM_CACHE_TYPES) {
    DLOG(ERROR) << "Size of cache_paths is invalid.";
    return;
  }

  if (!CreateCacheDirectories(cache_paths))
    return;

  // Change permissions of cache persistent directory to u+rwx,og+x in order to
  // allow archive files in that directory to be mounted by cros-disks.
  if (!ChangeFilePermissions(cache_paths[GDataCache::CACHE_TYPE_PERSISTENT],
                             S_IRWXU | S_IXGRP | S_IXOTH))
    return;

  DVLOG(1) << "Scanning directories";

  // Scan cache persistent and tmp directories to enumerate all files and create
  // corresponding entries for cache map.
  ScanCacheDirectory(cache_paths, GDataCache::CACHE_TYPE_PERSISTENT,
                     &cache_map_);
  ScanCacheDirectory(cache_paths, GDataCache::CACHE_TYPE_TMP, &cache_map_);

  // Then scan pinned and outgoing directories to update existing entries in
  // cache map, or create new ones for pinned symlinks to /dev/null which target
  // nothing.
  // Pinned and outgoing directories should be scanned after the persistent
  // directory as we'll add PINNED and DIRTY states respectively to the existing
  // files in the persistent directory per the contents of the pinned and
  // outgoing directories.
  ScanCacheDirectory(cache_paths, GDataCache::CACHE_TYPE_PINNED, &cache_map_);
  ScanCacheDirectory(cache_paths, GDataCache::CACHE_TYPE_OUTGOING, &cache_map_);

  DVLOG(1) << "Directory scan finished";
}

void GDataCacheMetadataMap::UpdateCache(const std::string& resource_id,
                                    const std::string& md5,
                                    GDataCache::CacheSubDirectoryType subdir,
                                    int cache_state) {
  AssertOnSequencedWorkerPool();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {  // New resource, create new entry.
    // Makes no sense to create new entry if cache state is NONE.
    DCHECK(cache_state != GDataCache::CACHE_STATE_NONE);
    if (cache_state != GDataCache::CACHE_STATE_NONE) {
      GDataCache::CacheEntry cache_entry(md5, subdir, cache_state);
      cache_map_.insert(std::make_pair(resource_id, cache_entry));
      DVLOG(1) << "Added res_id=" << resource_id
               << ", " << cache_entry.ToString();
    }
  } else {  // Resource exists.
    // If cache state is NONE, delete entry from cache map.
    if (cache_state == GDataCache::CACHE_STATE_NONE) {
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

void GDataCacheMetadataMap::RemoveFromCache(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter != cache_map_.end()) {
    // Delete the CacheEntry and remove it from the map.
    cache_map_.erase(iter);
  }
}

scoped_ptr<GDataCache::CacheEntry> GDataCacheMetadataMap::GetCacheEntry(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {
    DVLOG(1) << "Can't find " << resource_id << " in cache map";
    return scoped_ptr<GDataCache::CacheEntry>();
  }

  scoped_ptr<GDataCache::CacheEntry> cache_entry(
      new GDataCache::CacheEntry(iter->second));

  if (!CheckIfMd5Matches(md5, *cache_entry)) {
    DVLOG(1) << "Non-matching md5: want=" << md5
             << ", found=[res_id=" << resource_id
             << ", " << cache_entry->ToString()
             << "]";
    return scoped_ptr<GDataCache::CacheEntry>();
  }

  DVLOG(1) << "Found entry for res_id=" << resource_id
           << ", " << cache_entry->ToString();

  return cache_entry.Pass();
}

void GDataCacheMetadataMap::RemoveTemporaryFiles() {
  AssertOnSequencedWorkerPool();

  CacheMap::iterator iter = cache_map_.begin();
  while (iter != cache_map_.end()) {
    if (iter->second.sub_dir_type == GDataCache::CACHE_TYPE_TMP) {
      // Post-increment the iterator to avoid iterator invalidation.
      cache_map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void GDataCacheMetadataMap::ScanCacheDirectory(
    const std::vector<FilePath>& cache_paths,
    GDataCache::CacheSubDirectoryType sub_dir_type,
    CacheMap* cache_map) {
  file_util::FileEnumerator enumerator(
      cache_paths[sub_dir_type],
      false,  // not recursive
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::SHOW_SYM_LINKS),
      util::kWildCard);
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Extract resource_id and md5 from filename.
    std::string resource_id;
    std::string md5;
    std::string extra_extension;
    util::ParseCacheFilePath(current, &resource_id, &md5, &extra_extension);

    // Determine cache state.
    int cache_state = GDataCache::CACHE_STATE_NONE;
    // If we're scanning pinned directory and if entry already exists, just
    // update its pinned state.
    if (sub_dir_type == GDataCache::CACHE_TYPE_PINNED) {
      CacheMap::iterator iter = cache_map->find(resource_id);
      if (iter != cache_map->end()) {  // Entry exists, update pinned state.
        iter->second.cache_state =
            GDataCache::SetCachePinned(iter->second.cache_state);
        continue;
      }
      // Entry doesn't exist, this is a special symlink that refers to
      // /dev/null; follow through to create an entry with the PINNED but not
      // PRESENT state.
      cache_state = GDataCache::SetCachePinned(cache_state);
    } else if (sub_dir_type == GDataCache::CACHE_TYPE_OUTGOING) {
      // If we're scanning outgoing directory, entry must exist, update its
      // dirty state.
      // If entry doesn't exist, it's a logic error from previous execution,
      // ignore this outgoing symlink and move on.
      CacheMap::iterator iter = cache_map->find(resource_id);
      if (iter != cache_map->end()) {  // Entry exists, update dirty state.
        iter->second.cache_state =
            GDataCache::SetCacheDirty(iter->second.cache_state);
      } else {
        NOTREACHED() << "Dirty cache file MUST have actual file blob";
      }
      continue;
    } else if (extra_extension == util::kMountedArchiveFileExtension) {
      // Mounted archives in cache should be unmounted upon logout/shutdown.
      // But if we encounter a mounted file at start, delete it and create an
      // entry with not PRESENT state.
      DCHECK(sub_dir_type == GDataCache::CACHE_TYPE_PERSISTENT);
      file_util::Delete(current, false);
    } else {
      // Scanning other directories means that cache file is actually present.
      cache_state = GDataCache::SetCachePresent(cache_state);
    }

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(
        resource_id, GDataCache::CacheEntry(md5, sub_dir_type, cache_state)));
  }
}

// static
bool GDataCacheMetadataMap::CheckIfMd5Matches(
    const std::string& md5,
    const GDataCache::CacheEntry& cache_entry) {
  if (cache_entry.IsDirty()) {
    // If the entry is dirty, its MD5 may have been replaced by "local"
    // during cache initialization, so we don't compare MD5.
    return true;
  } else if (cache_entry.IsPinned() && cache_entry.md5.empty()) {
    // If the entry is pinned, it's ok for the entry to have an empty
    // MD5. This can happen if the pinned file is not fetched. MD5 for pinned
    // files are collected from files in "persistent" directory, but the
    // persistent files do not exisit if these are not fetched yet.
    return true;
  } else if (md5.empty()) {
    // If the MD5 matching is not requested, don't check MD5.
    return true;
  } else if (md5 == cache_entry.md5) {
    // Otherwise, compare the MD5.
    return true;
  }
  return false;
}

}  // namespace gdata
