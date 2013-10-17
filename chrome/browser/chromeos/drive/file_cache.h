// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/drive/drive_service_interface.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class FileCacheEntry;

namespace internal {

// Callback for GetFileFromCache.
typedef base::Callback<void(FileError error,
                            const base::FilePath& cache_file_path)>
    GetFileFromCacheCallback;

// Interface class used for getting the free disk space. Tests can inject an
// implementation that reports fake free disk space.
class FreeDiskSpaceGetterInterface {
 public:
  virtual ~FreeDiskSpaceGetterInterface() {}
  virtual int64 AmountOfFreeDiskSpace() = 0;
};

// FileCache is used to maintain cache states of FileSystem.
//
// All non-static public member functions, unless mentioned otherwise (see
// GetCacheFilePath() for example), should be run with |blocking_task_runner|.
class FileCache {
 public:
  // Enum defining type of file operation e.g. copy or move, etc.
  enum FileOperationType {
    FILE_OPERATION_MOVE = 0,
    FILE_OPERATION_COPY,
  };

  typedef ResourceMetadataStorage::CacheEntryIterator Iterator;

  // |cache_file_directory| stores cached files.
  //
  // |blocking_task_runner| is used to post a task to the blocking worker
  // pool for file operations. Must not be null.
  //
  // |free_disk_space_getter| is used to inject a custom free disk space
  // getter for testing. NULL must be passed for production code.
  //
  // Must be called on the UI thread.
  FileCache(ResourceMetadataStorage* storage,
            const base::FilePath& cache_file_directory,
            base::SequencedTaskRunner* blocking_task_runner,
            FreeDiskSpaceGetterInterface* free_disk_space_getter);

  // Returns true if the given path is under drive cache directory, i.e.
  // <user_profile_dir>/GCache/v1
  //
  // Can be called on any thread.
  bool IsUnderFileCacheDirectory(const base::FilePath& path) const;

  // Gets the cache entry for file corresponding to |id| and returns true if
  // entry exists in cache map.
  bool GetCacheEntry(const std::string& id, FileCacheEntry* entry);

  // Returns an object to iterate over entries.
  scoped_ptr<Iterator> GetIterator();

  // Frees up disk space to store a file with |num_bytes| size content, while
  // keeping kMinFreeSpace bytes on the disk, if needed.
  // Returns true if we successfully manage to have enough space, otherwise
  // false.
  bool FreeDiskSpaceIfNeededFor(int64 num_bytes);

  // Checks if file corresponding to |id| exists in cache, and returns
  // FILE_ERROR_OK with |cache_file_path| storing the path to the file.
  // |cache_file_path| must not be null.
  FileError GetFile(const std::string& id, base::FilePath* cache_file_path);

  // Stores |source_path| as a cache of the remote content of the file
  // with |id| and |md5|.
  FileError Store(const std::string& id,
                  const std::string& md5,
                  const base::FilePath& source_path,
                  FileOperationType file_operation_type);

  // Runs Pin() on |blocking_task_runner_|, and calls |callback| with the result
  // asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void PinOnUIThread(const std::string& id,
                     const FileOperationCallback& callback);

  // Pins the specified entry.
  FileError Pin(const std::string& id);

  // Runs Unpin() on |blocking_task_runner_|, and calls |callback| with the
  // result asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void UnpinOnUIThread(const std::string& id,
                       const FileOperationCallback& callback);

  // Unpins the specified entry.
  FileError Unpin(const std::string& id);

  // Runs MarkAsMounted() on |blocking_task_runner_|, and calls |callback| with
  // the result asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void MarkAsMountedOnUIThread(const std::string& id,
                               const GetFileFromCacheCallback& callback);

  // Sets the state of the cache entry corresponding to |id| as mounted.
  FileError MarkAsMounted(const std::string& id,
                          base::FilePath* cache_file_path);

  // Set the state of the cache entry corresponding to file_path as unmounted.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void MarkAsUnmountedOnUIThread(const base::FilePath& file_path,
                                 const FileOperationCallback& callback);

  // Marks the specified entry dirty.
  FileError MarkDirty(const std::string& id);

  // Clears dirty state of the specified entry and updates its MD5.
  FileError ClearDirty(const std::string& id, const std::string& md5);

  // Removes the specified cache entry and delete cache files if available.
  FileError Remove(const std::string& id);

  // Removes all the files in the cache directory and cache entries in DB.
  bool ClearAll();

  // Initializes the cache. Returns true on success.
  bool Initialize();

  // Destroys this cache. This function posts a task to the blocking task
  // runner to safely delete the object.
  // Must be called on the UI thread.
  void Destroy();

  // Converts entry IDs and cache file names to the desired format.
  // TODO(hashimoto): Remove this method at some point.
  bool CanonicalizeIDs(const ResourceIdCanonicalizer& id_canonicalizer);

 private:
  friend class FileCacheTest;
  friend class FileCacheTestOnUIThread;

  ~FileCache();

  // Returns absolute path of the file if it were cached or to be cached.
  //
  // Can be called on any thread.
  base::FilePath GetCacheFilePath(const std::string& id) const;

  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

  // Destroys the cache on the blocking pool.
  void DestroyOnBlockingPool();

  // Used to implement Store and StoreLocallyModifiedOnUIThread.
  // TODO(hidehiko): Merge this method with Store(), after
  // StoreLocallyModifiedOnUIThread is removed.
  FileError StoreInternal(const std::string& id,
                          const std::string& md5,
                          const base::FilePath& source_path,
                          FileOperationType file_operation_type);

  // Used to implement MarkAsUnmountedOnUIThread.
  FileError MarkAsUnmounted(const base::FilePath& file_path);

  // Returns true if we have sufficient space to store the given number of
  // bytes, while keeping kMinFreeSpace bytes on the disk.
  bool HasEnoughSpaceFor(int64 num_bytes, const base::FilePath& path);

  // Renames cache files from old "id.md5" format to the new format.
  // TODO(hashimoto): Remove this method at some point.
  void RenameCacheFilesToNewFormat();

  const base::FilePath cache_file_directory_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  ResourceMetadataStorage* storage_;

  FreeDiskSpaceGetterInterface* free_disk_space_getter_;  // Not owned.

  // IDs of files marked mounted.
  std::set<std::string> mounted_files_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileCache> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

// The minimum free space to keep. Operations that add cache files return
// FILE_ERROR_NO_LOCAL_SPACE if the available space is smaller than
// this value.
//
// Copied from cryptohome/homedirs.h.
// TODO(satorux): Share the constant.
const int64 kMinFreeSpace = 512 * 1LL << 20;

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_
