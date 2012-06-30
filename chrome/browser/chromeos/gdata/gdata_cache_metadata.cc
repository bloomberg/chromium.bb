// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache_metadata.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"

namespace gdata {

namespace {

// Changes the permissions of |file_path| to |permissions|.
bool ChangeFilePermissions(const FilePath& file_path, mode_t permissions) {
  if (HANDLE_EINTR(chmod(file_path.value().c_str(), permissions)) != 0) {
    PLOG(ERROR) << "Error changing permissions of " << file_path.value();
    return false;
  }
  DVLOG(1) << "Changed permissions of " << file_path.value();
  return true;
}

// Returns true if |file_path| is a valid symbolic link as |sub_dir_type|.
// Otherwise, returns false with the reason.
bool IsValidSymbolicLink(const FilePath& file_path,
                         GDataCache::CacheSubDirectoryType sub_dir_type,
                         const std::vector<FilePath>& cache_paths,
                         std::string* reason) {
  DCHECK(sub_dir_type == GDataCache::CACHE_TYPE_PINNED ||
         sub_dir_type == GDataCache::CACHE_TYPE_OUTGOING);

  FilePath destination;
  if (!file_util::ReadSymbolicLink(file_path, &destination)) {
    *reason = "failed to read the symlink (maybe not a symlink)";
    return false;
  }

  if (!file_util::PathExists(destination)) {
    *reason = "pointing to a non-existent file";
    return false;
  }

  // pinned-but-not-fetched files are symlinks to kSymLinkToDevNull.
  if (sub_dir_type == GDataCache::CACHE_TYPE_PINNED &&
      destination == FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull)) {
    return true;
  }

  // The destination file should be in the persistent directory.
  if (!cache_paths[GDataCache::CACHE_TYPE_PERSISTENT].IsParent(destination)) {
    *reason = "pointing to a file outside of persistent directory";
    return false;
  }

  return true;
}

// Remove invalid files from persistent directory.
//
// 1) dirty-but-not-committed files. The dirty files should be committed
// (i.e. symlinks created in 'outgoing' directory) before shutdown, but the
// symlinks may not be created if the system shuts down unexpectedly.
//
// 2) neither dirty nor pinned. Files in the persistent directory should be
// in the either of the states.
void RemoveInvalidFilesFromPersistentDirectory(
    const GDataCacheMetadataMap::ResourceIdToFilePathMap& persistent_file_map,
    const GDataCacheMetadataMap::ResourceIdToFilePathMap& outgoing_file_map,
    GDataCacheMetadataMap::CacheMap* cache_map) {
  for (GDataCacheMetadataMap::ResourceIdToFilePathMap::const_iterator iter =
           persistent_file_map.begin();
       iter != persistent_file_map.end(); ++iter) {
    const std::string& resource_id = iter->first;
    const FilePath& file_path = iter->second;

    GDataCacheMetadataMap::CacheMap::iterator cache_map_iter =
        cache_map->find(resource_id);
    if (cache_map_iter != cache_map->end()) {
      const GDataCache::CacheEntry& cache_entry = cache_map_iter->second;
      // If the file is dirty but not committed, remove it.
      if (cache_entry.IsDirty() && outgoing_file_map.count(resource_id) == 0) {
        LOG(WARNING) << "Removing dirty-but-not-committed file: "
                     << file_path.value();
        file_util::Delete(file_path, false);
        cache_map->erase(cache_map_iter);
      } else if (!cache_entry.IsDirty() && !cache_entry.IsPinned()) {
        // If the file is neither dirty nor pinned, remove it.
        LOG(WARNING) << "Removing persistent-but-dangling file: "
                     << file_path.value();
        file_util::Delete(file_path, false);
        cache_map->erase(cache_map_iter);
      }
    }
  }
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
    LOG(ERROR) << "Size of cache_paths is invalid.";
    return;
  }

  if (!GDataCache::CreateCacheDirectories(cache_paths))
    return;

  // Change permissions of cache persistent directory to u+rwx,og+x in order to
  // allow archive files in that directory to be mounted by cros-disks.
  if (!ChangeFilePermissions(cache_paths[GDataCache::CACHE_TYPE_PERSISTENT],
                             S_IRWXU | S_IXGRP | S_IXOTH))
    return;

  DVLOG(1) << "Scanning directories";

  // Scan cache persistent and tmp directories to enumerate all files and create
  // corresponding entries for cache map.
  ResourceIdToFilePathMap persistent_file_map;
  ScanCacheDirectory(cache_paths,
                     GDataCache::CACHE_TYPE_PERSISTENT,
                     &cache_map_,
                     &persistent_file_map);
  ResourceIdToFilePathMap tmp_file_map;
  ScanCacheDirectory(cache_paths,
                     GDataCache::CACHE_TYPE_TMP,
                     &cache_map_,
                     &tmp_file_map);

  // Then scan pinned directory to update existing entries in cache map, or
  // create new ones for pinned symlinks to /dev/null which target nothing.
  //
  // Pinned directory should be scanned after the persistent directory as
  // we'll add PINNED states to the existing files in the persistent
  // directory per the contents of the pinned directory.
  ResourceIdToFilePathMap pinned_file_map;
  ScanCacheDirectory(cache_paths,
                     GDataCache::CACHE_TYPE_PINNED,
                     &cache_map_,
                     &pinned_file_map);
  // Then scan outgoing directory to check if dirty-files are committed
  // properly (i.e. symlinks created in outgoing directory).
  ResourceIdToFilePathMap outgoing_file_map;
  ScanCacheDirectory(cache_paths,
                     GDataCache::CACHE_TYPE_OUTGOING,
                     &cache_map_,
                     &outgoing_file_map);

  RemoveInvalidFilesFromPersistentDirectory(persistent_file_map,
                                            outgoing_file_map,
                                            &cache_map_);
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

void GDataCacheMetadataMap::Iterate(const IterateCallback& callback) {
  AssertOnSequencedWorkerPool();

  for (CacheMap::const_iterator iter = cache_map_.begin();
       iter != cache_map_.end(); ++iter) {
    callback.Run(iter->first, iter->second);
  }
}

// static
void GDataCacheMetadataMap::ScanCacheDirectory(
    const std::vector<FilePath>& cache_paths,
    GDataCache::CacheSubDirectoryType sub_dir_type,
    CacheMap* cache_map,
    ResourceIdToFilePathMap* processed_file_map) {
  DCHECK(cache_map);
  DCHECK(processed_file_map);

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
      std::string reason;
      if (!IsValidSymbolicLink(current, sub_dir_type, cache_paths, &reason)) {
        LOG(WARNING) << "Removing an invalid symlink: " << current.value()
                     << ": " << reason;
        util::DeleteSymlink(current);
        continue;
      }

      CacheMap::iterator iter = cache_map->find(resource_id);
      if (iter != cache_map->end()) {  // Entry exists, update pinned state.
        iter->second.cache_state =
            GDataCache::SetCachePinned(iter->second.cache_state);

        processed_file_map->insert(std::make_pair(resource_id, current));
        continue;
      }
      // Entry doesn't exist, this is a special symlink that refers to
      // /dev/null; follow through to create an entry with the PINNED but not
      // PRESENT state.
      cache_state = GDataCache::SetCachePinned(cache_state);
    } else if (sub_dir_type == GDataCache::CACHE_TYPE_OUTGOING) {
      std::string reason;
      if (!IsValidSymbolicLink(current, sub_dir_type, cache_paths, &reason)) {
        LOG(WARNING) << "Removing an invalid symlink: " << current.value()
                     << ": " << reason;
        util::DeleteSymlink(current);
        continue;
      }

      // If we're scanning outgoing directory, entry must exist and be dirty.
      // Otherwise, it's a logic error from previous execution, remove this
      // outgoing symlink and move on.
      CacheMap::iterator iter = cache_map->find(resource_id);
      if (iter == cache_map->end() || !iter->second.IsDirty()) {
        LOG(WARNING) << "Removing an symlink to a non-dirty file: "
                     << current.value();
        util::DeleteSymlink(current);
        continue;
      }

      processed_file_map->insert(std::make_pair(resource_id, current));
      continue;
    } else if (sub_dir_type == GDataCache::CACHE_TYPE_PERSISTENT ||
               sub_dir_type == GDataCache::CACHE_TYPE_TMP) {
      if (file_util::IsLink(current)) {
        LOG(WARNING) << "Removing a symlink in persistent/tmp directory"
                     << current.value();
        util::DeleteSymlink(current);
        continue;
      }
      if (extra_extension == util::kMountedArchiveFileExtension) {
        // Mounted archives in cache should be unmounted upon logout/shutdown.
        // But if we encounter a mounted file at start, delete it and create an
        // entry with not PRESENT state.
        DCHECK(sub_dir_type == GDataCache::CACHE_TYPE_PERSISTENT);
        file_util::Delete(current, false);
      } else {
        // The cache file is present.
        cache_state = GDataCache::SetCachePresent(cache_state);

        // Adds the dirty bit if |md5| indicates that the file is dirty, and
        // the file is in the persistent directory.
        if (md5 == util::kLocallyModifiedFileExtension) {
          if (sub_dir_type == GDataCache::CACHE_TYPE_PERSISTENT) {
            cache_state |= GDataCache::SetCacheDirty(cache_state);
          } else {
            LOG(WARNING) << "Removing a dirty file in tmp directory: "
                         << current.value();
            file_util::Delete(current, false);
            continue;
          }
        }
      }
    } else {
      NOTREACHED() << "Unexpected sub directory type: " << sub_dir_type;
    }

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(
        resource_id, GDataCache::CacheEntry(md5, sub_dir_type, cache_state)));
    processed_file_map->insert(std::make_pair(resource_id, current));
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
