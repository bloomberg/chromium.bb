// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#pragma once

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"

class Profile;

namespace gdata {

class GDataCacheMetadata;

// Callback for SetMountedStateOnUIThread.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& file_path)> SetMountedStateCallback;

// Callback for completion of cache operation.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5)> CacheOperationCallback;

// Callback for GetFileFromCache.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& cache_file_path)>
    GetFileFromCacheCallback;

// GDataCache is used to maintain cache states of GDataFileSystem.
//
// All non-static public member functions, unless mentioned otherwise (see
// GetCacheFilePath() for example), should be called from the sequenced
// worker pool with the sequence token set by CreateGDataCacheOnUIThread(). This
// threading model is enforced by AssertOnSequencedWorkerPool().
//
// TODO(hashimoto): Change threading model of this class to make public methods
// being called on UI thread unless mentioned otherwise. crbug.com/132926
class GDataCache {
 public:
  // Enum defining GCache subdirectory location.
  // This indexes into |GDataCache::cache_paths_| vector.
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

  // Enum defining origin of a cached file.
  enum CachedFileOrigin {
    CACHED_FILE_FROM_SERVER = 0,
    CACHED_FILE_LOCALLY_MODIFIED,
    CACHED_FILE_MOUNTED,
  };

  // Enum defining type of file operation e.g. copy or move, etc.
  enum FileOperationType {
    FILE_OPERATION_MOVE = 0,
    FILE_OPERATION_COPY,
  };

  // Used to notify events. All events are notified on UI thread.
  class Observer {
   public:
    // Triggered when a file has been pinned successfully.
    virtual void OnCachePinned(const std::string& resource_id,
                               const std::string& md5) {}

    // Triggered when a file has been unpinned successfully.
    virtual void OnCacheUnpinned(const std::string& resource_id,
                                 const std::string& md5) {}

    // Triggered when a dirty file has been committed (saved) successfully.
    virtual void OnCacheCommitted(const std::string& resource_id) {}

   protected:
    virtual ~Observer() {}
  };

  // Structure to store information of an existing cache file.
  struct CacheEntry {
    CacheEntry() : sub_dir_type(CACHE_TYPE_META),
                   cache_state(0) {}

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

  // Returns the sub-directory under gdata cache directory for the given sub
  // directory type. Example:  <user_profile_dir>/GCache/v1/tmp
  //
  // Can be called on any thread.
  FilePath GetCacheDirectoryPath(CacheSubDirectoryType sub_dir_type) const;

  // Returns absolute path of the file if it were cached or to be cached.
  //
  // Can be called on any thread.
  FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            CacheSubDirectoryType sub_dir_type,
                            CachedFileOrigin file_orign) const;

  // Returns true if the given path is under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1
  //
  // Can be called on any thread.
  bool IsUnderGDataCacheDirectory(const FilePath& path) const;

  // Adds observer.
  // Must be called on UI thread.
  void AddObserver(Observer* observer);

  // Removes observer.
  // Must be called on UI thread.
  void RemoveObserver(Observer* observer);

  // Frees up disk space to store the given number of bytes, while keeping
  // kMinFreSpace bytes on the disk, if needed.  |has_enough_space| is
  // updated to indicate if we have enough space.
  void FreeDiskSpaceIfNeededFor(int64 num_bytes,
                                bool* has_enough_space);

  // Checks if file corresponding to |resource_id| and |md5| exists in cache.
  void GetFileOnUIThread(const std::string& resource_id,
                         const std::string& md5,
                         const GetFileFromCacheCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves or copies (per |file_operation_type|) |source_path|
  //   to |dest_path| in the cache dir
  // - if necessary, creates symlink
  // - deletes stale cached versions of |resource_id| in
  // |dest_path|'s directory.
  void StoreOnUIThread(const std::string& resource_id,
                       const std::string& md5,
                       const FilePath& source_path,
                       FileOperationType file_operation_type,
                       const CacheOperationCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is not dirty
  // - creates symlink in pinned dir that references downloaded or locally
  //   modified file
  void PinOnUIThread(const std::string& resource_id,
                     const std::string& md5,
                     const CacheOperationCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in tmp dir if file is not dirty
  // - deletes symlink from pinned dir
  void UnpinOnUIThread(const std::string& resource_id,
                       const std::string& md5,
                       const CacheOperationCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path|, where
  //   if we're mounting: |source_path| is the unmounted path and has .<md5>
  //       extension, and |dest_path| is the mounted path in persistent dir
  //       and has .<md5>.mounted extension;
  //   if we're unmounting: the opposite is true for the two paths, i.e.
  //       |dest_path| is the mounted path and |source_path| the unmounted path.
  void SetMountedStateOnUIThread(const FilePath& file_path,
                                 bool to_mount,
                                 const SetMountedStateCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir, where
  //   |source_path| has .<md5> extension and |dest_path| has .local extension
  // - if file is pinned, updates symlink in pinned dir to reference dirty file
  void MarkDirtyOnUIThread(const std::string& resource_id,
                           const std::string& md5,
                           const GetFileFromCacheCallback& callback);

  // Modifies cache state, i.e. creates symlink in outgoing
  // dir to reference dirty file in persistent dir.
  void CommitDirtyOnUIThread(const std::string& resource_id,
                             const std::string& md5,
                             const CacheOperationCallback& callback);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is pinned or tmp dir otherwise, where |source_path| has .local
  //   extension and |dest_path| has .<md5> extension
  // - deletes symlink in outgoing dir
  // - if file is pinned, updates symlink in pinned dir to reference
  //   |dest_path|
  void ClearDirtyOnUIThread(const std::string& resource_id,
                            const std::string& md5,
                            const CacheOperationCallback& callback);

  // Does the following:
  // - remove all delete stale cache versions corresponding to |resource_id| in
  //   persistent, tmp and pinned directories
  // - remove entry corresponding to |resource_id| from cache map.
  void RemoveOnUIThread(const std::string& resource_id,
                        const CacheOperationCallback& callback);

  // Utility method to call Initialize on UI thread.
  void RequestInitializeOnUIThread();

  // Returns the cache entry for file corresponding to |resource_id| and |md5|
  // if entry exists in cache map.  Otherwise, returns NULL.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  scoped_ptr<CacheEntry> GetCacheEntry(const std::string& resource_id,
                                       const std::string& md5);

  // Factory methods for GDataCache.
  // |pool| and |sequence_token| are used to assert that the functions are
  // called on the right sequenced worker pool with the right sequence token.
  //
  // For testing, the thread assertion can be disabled by passing NULL and
  // the default value of SequenceToken.
  static GDataCache* CreateGDataCacheOnUIThread(
      const FilePath& cache_root_path,
      base::SequencedWorkerPool* pool,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);

  // Deletes the cache.
  void DestroyOnUIThread();

  // Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
  // profile.
  // TODO(satorux): Write a unit test for this.
  static FilePath GetCacheRootPath(Profile* profile);

 private:
  GDataCache(
      const FilePath& cache_root_path,
      base::SequencedWorkerPool* pool_,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);
  virtual ~GDataCache();

  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

  // Initializes the cache.
  void Initialize();

  // Deletes the cache.
  void Destroy();

  // Used to implement GetFileOnUIThread.
  void GetFile(const std::string& resource_id,
               const std::string& md5,
               base::PlatformFileError* error,
               FilePath* cache_file_path);

  // Used to implement StoreOnUIThread.
  void Store(const std::string& resource_id,
             const std::string& md5,
             const FilePath& source_path,
             FileOperationType file_operation_type,
             base::PlatformFileError* error);

  // Used to implement PinOnUIThread.
  void Pin(const std::string& resource_id,
           const std::string& md5,
           FileOperationType file_operation_type,
           base::PlatformFileError* error);

  // Used to implement UnpinOnUIThread.
  void Unpin(const std::string& resource_id,
             const std::string& md5,
             FileOperationType file_operation_type,
             base::PlatformFileError* error);

  // Used to implement SetMountedStateOnUIThread.
  void SetMountedState(const FilePath& file_path,
                       bool to_mount,
                       base::PlatformFileError* error,
                       FilePath* cache_file_path);

  // Used to implement MarkDirtyOnUIThread.
  void MarkDirty(const std::string& resource_id,
                 const std::string& md5,
                 FileOperationType file_operation_type,
                 base::PlatformFileError* error,
                 FilePath* cache_file_path);

  // Used to implement CommitDirtyOnUIThread.
  void CommitDirty(const std::string& resource_id,
                   const std::string& md5,
                   FileOperationType file_operation_type,
                   base::PlatformFileError* error);

  // Used to implement ClearDirtyOnUIThread.
  void ClearDirty(const std::string& resource_id,
                  const std::string& md5,
                  FileOperationType file_operation_type,
                  base::PlatformFileError* error);

  // Used to implement RemoveOnUIThread.
  void Remove(const std::string& resource_id,
              base::PlatformFileError* error);

  // Runs callback and notifies the observers when file is pinned.
  void OnPinned(base::PlatformFileError* error,
                const std::string& resource_id,
                const std::string& md5,
                const CacheOperationCallback& callback);

  // Runs callback and notifies the observers when file is unpinned.
  void OnUnpinned(base::PlatformFileError* error,
                  const std::string& resource_id,
                  const std::string& md5,
                  const CacheOperationCallback& callback);

  // Runs callback and notifies the observers when file is committed.
  void OnCommitDirty(base::PlatformFileError* error,
                     const std::string& resource_id,
                     const std::string& md5,
                     const CacheOperationCallback& callback);

  // The root directory of the cache (i.e. <user_profile_dir>/GCache/v1).
  const FilePath cache_root_path_;
  // Paths for all subdirectories of GCache, one for each
  // GDataCache::CacheSubDirectoryType enum.
  const std::vector<FilePath> cache_paths_;
  base::SequencedWorkerPool* pool_;
  const base::SequencedWorkerPool::SequenceToken sequence_token_;

  // The cache state data. This member must be access only on the blocking pool.
  scoped_ptr<GDataCacheMetadata> metadata_;

  // WeakPtrFactory and WeakPtr bound to the UI thread.
  base::WeakPtrFactory<GDataCache> ui_weak_ptr_factory_;
  base::WeakPtr<GDataCache> ui_weak_ptr_;

  // List of observers, this member must be accessed on UI thread.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(GDataCache);
};


// The minimum free space to keep. GDataFileSystem::GetFileByPath() returns
// base::PLATFORM_FILE_ERROR_NO_SPACE if the available space is smaller than
// this value.
//
// Copied from cryptohome/homedirs.h.
// TODO(satorux): Share the constant.
const int64 kMinFreeSpace = 512 * 1LL << 20;

// Interface class used for getting the free disk space. Only for testing.
class FreeDiskSpaceGetterInterface {
 public:
  virtual ~FreeDiskSpaceGetterInterface() {}
  virtual int64 AmountOfFreeDiskSpace() const = 0;
};

// Sets the free disk space getter for testing.
// The existing getter is deleted.
void SetFreeDiskSpaceGetterForTesting(
    FreeDiskSpaceGetterInterface* getter);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
