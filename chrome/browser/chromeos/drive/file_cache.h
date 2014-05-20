// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

namespace base {
class ScopedClosureRunner;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

namespace internal {

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

  // |cache_file_directory| stores cached files.
  //
  // |blocking_task_runner| indicates the blocking worker pool for cache
  // operations. All operations on this FileCache must be run on this runner.
  // Must not be null.
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

  // Frees up disk space to store a file with |num_bytes| size content, while
  // keeping cryptohome::kMinFreeSpaceInBytes bytes on the disk, if needed.
  // Returns true if we successfully manage to have enough space, otherwise
  // false.
  bool FreeDiskSpaceIfNeededFor(int64 num_bytes);

  // Checks if file corresponding to |id| exists in cache, and returns
  // FILE_ERROR_OK with |cache_file_path| storing the path to the file.
  // |cache_file_path| must not be null.
  FileError GetFile(const std::string& id, base::FilePath* cache_file_path);

  // Stores |source_path| as a cache of the remote content of the file
  // with |id| and |md5|.
  // Pass an empty string as MD5 to mark the entry as dirty.
  FileError Store(const std::string& id,
                  const std::string& md5,
                  const base::FilePath& source_path,
                  FileOperationType file_operation_type);

  // Pins the specified entry.
  FileError Pin(const std::string& id);

  // Unpins the specified entry.
  FileError Unpin(const std::string& id);

  // Sets the state of the cache entry corresponding to |id| as mounted.
  FileError MarkAsMounted(const std::string& id,
                          base::FilePath* cache_file_path);

  // Sets the state of the cache entry corresponding to file_path as unmounted.
  FileError MarkAsUnmounted(const base::FilePath& file_path);

  // Opens the cache file corresponding to |id| for write. |file_closer| should
  // be kept alive until writing finishes.
  // This method must be called before writing to cache files.
  FileError OpenForWrite(const std::string& id,
                         scoped_ptr<base::ScopedClosureRunner>* file_closer);

  // Returns true if the cache file corresponding to |id| is write-opened.
  bool IsOpenedForWrite(const std::string& id);

  // Calculates MD5 of the cache file and updates the stored value.
  FileError UpdateMd5(const std::string& id);

  // Clears dirty state of the specified entry.
  FileError ClearDirty(const std::string& id);

  // Removes the specified cache entry and delete cache files if available.
  FileError Remove(const std::string& id);

  // Removes all the files in the cache directory.
  bool ClearAll();

  // Initializes the cache. Returns true on success.
  bool Initialize();

  // Destroys this cache. This function posts a task to the blocking task
  // runner to safely delete the object.
  // Must be called on the UI thread.
  void Destroy();

  // Moves files in the cache directory which are not managed by FileCache to
  // |dest_directory|.
  // |recovered_cache_info| should contain cache info recovered from the trashed
  // metadata DB. It is used to ignore non-dirty files.
  bool RecoverFilesFromCacheDirectory(
      const base::FilePath& dest_directory,
      const ResourceMetadataStorage::RecoveredCacheInfoMap&
          recovered_cache_info);

 private:
  friend class FileCacheTest;

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

  // Returns true if we have sufficient space to store the given number of
  // bytes, while keeping cryptohome::kMinFreeSpaceInBytes bytes on the disk.
  bool HasEnoughSpaceFor(int64 num_bytes, const base::FilePath& path);

  // Renames cache files from old "prefix:id.md5" format to the new format.
  // TODO(hashimoto): Remove this method at some point.
  bool RenameCacheFilesToNewFormat();

  // This method must be called after writing to a cache file.
  // Used to implement OpenForWrite().
  void CloseForWrite(const std::string& id);

  const base::FilePath cache_file_directory_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  ResourceMetadataStorage* storage_;

  FreeDiskSpaceGetterInterface* free_disk_space_getter_;  // Not owned.

  // IDs of files being write-opened.
  std::map<std::string, int> write_opened_files_;

  // IDs of files marked mounted.
  std::set<std::string> mounted_files_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  // This object should be accessed only on |blocking_task_runner_|.
  base::WeakPtrFactory<FileCache> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_CACHE_H_
