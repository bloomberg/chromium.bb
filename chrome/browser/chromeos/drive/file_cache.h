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

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class FileCacheEntry;

// Callback for GetCacheEntry.
// |success| indicates if the operation was successful.
// |cache_entry| is the obtained cache entry. On failure, |cache_state| is
// set to TEST_CACHE_STATE_NONE.
typedef base::Callback<void(bool success, const FileCacheEntry& cache_entry)>
    GetCacheEntryCallback;

// Callback for Iterate().
typedef base::Callback<void(const std::string& id,
                            const FileCacheEntry& cache_entry)>
    CacheIterateCallback;

namespace internal {

// Callback for GetFileFromCache.
typedef base::Callback<void(FileError error,
                            const base::FilePath& cache_file_path)>
    GetFileFromCacheCallback;

// Callback for ClearAllOnUIThread.
// |success| indicates if the operation was successful.
// TODO(satorux): Change this to FileError when it becomes necessary.
typedef base::Callback<void(bool success)> ClearAllCallback;

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

  // Gets the cache entry for file corresponding to |id| and runs
  // |callback| with true and the entry found if entry exists in cache map.
  // Otherwise, runs |callback| with false.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void GetCacheEntryOnUIThread(const std::string& id,
                               const GetCacheEntryCallback& callback);

  // Gets the cache entry by the given ID.
  // See also GetCacheEntryOnUIThread().
  bool GetCacheEntry(const std::string& id, FileCacheEntry* entry);

  // Runs Iterate() with |iteration_callback| on |blocking_task_runner_| and
  // runs |completion_callback| upon completion.
  // Must be called on UI thread.
  void IterateOnUIThread(const CacheIterateCallback& iteration_callback,
                         const base::Closure& completion_callback);

  // Returns an object to iterate over entries.
  scoped_ptr<Iterator> GetIterator();

  // Frees up disk space to store a file with |num_bytes| size content, while
  // keeping kMinFreeSpace bytes on the disk, if needed.
  // Returns true if we successfully manage to have enough space, otherwise
  // false.
  bool FreeDiskSpaceIfNeededFor(int64 num_bytes);

  // Runs GetFile() on |blocking_task_runner_|, and calls |callback| with
  // the result asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void GetFileOnUIThread(const std::string& id,
                         const GetFileFromCacheCallback& callback);

  // Checks if file corresponding to |id| exists in cache, and returns
  // FILE_ERROR_OK with |cache_file_path| storing the path to the file.
  // |cache_file_path| must not be null.
  FileError GetFile(const std::string& id, base::FilePath* cache_file_path);

  // Runs Store() on |blocking_task_runner_|, and calls |callback| with
  // the result asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void StoreOnUIThread(const std::string& id,
                       const std::string& md5,
                       const base::FilePath& source_path,
                       FileOperationType file_operation_type,
                       const FileOperationCallback& callback);

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

  // Sets the state of the cache entry corresponding to |id| as
  // mounted.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void MarkAsMountedOnUIThread(const std::string& id,
                               const GetFileFromCacheCallback& callback);

  // Set the state of the cache entry corresponding to file_path as unmounted.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void MarkAsUnmountedOnUIThread(const base::FilePath& file_path,
                                 const FileOperationCallback& callback);

  // Runs MarkDirty() on |blocking_task_runner_|, and calls |callback| with the
  // result asynchronously.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void MarkDirtyOnUIThread(const std::string& id,
                           const FileOperationCallback& callback);

  // Marks the specified entry dirty.
  FileError MarkDirty(const std::string& id);

  // Clears dirty state of the specified entry and updates its MD5.
  FileError ClearDirty(const std::string& id, const std::string& md5);

  // Runs Remove() on |blocking_task_runner_| and runs |callback| with the
  // result.
  // Must be called on the UI thread.
  void RemoveOnUIThread(const std::string& id,
                        const FileOperationCallback& callback);

  // Removes the specified cache entry and delete cache files if available.
  // Synchronous version of RemoveOnUIThread().
  FileError Remove(const std::string& id);

  // Does the following:
  // - remove all the files in the cache directory.
  // - re-create the |metadata_| instance.
  // |callback| must not be null.
  // Must be called on the UI thread.
  void ClearAllOnUIThread(const ClearAllCallback& callback);

  // Initializes the cache. Returns true on success.
  bool Initialize();

  // Destroys this cache. This function posts a task to the blocking task
  // runner to safely delete the object.
  // Must be called on the UI thread.
  void Destroy();

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

  // Used to implement MarkAsMountedOnUIThread.
  FileError MarkAsMounted(const std::string& id,
                          base::FilePath* cache_file_path);

  // Used to implement MarkAsUnmountedOnUIThread.
  FileError MarkAsUnmounted(const base::FilePath& file_path);

  // Used to implement ClearAllOnUIThread.
  bool ClearAll();

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
