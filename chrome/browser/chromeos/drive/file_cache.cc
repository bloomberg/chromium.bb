// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

typedef std::map<std::string, FileCacheEntry> CacheMap;

// Returns resource ID extracted from the path.
std::string GetResourceIdFromPath(const base::FilePath& path) {
  return util::UnescapeCacheFileName(path.BaseName().AsUTF8Unsafe());
}

// Scans cache subdirectory and insert found files to |cache_map|.
void ScanCacheDirectory(const base::FilePath& directory_path,
                        CacheMap* cache_map) {
  base::FileEnumerator enumerator(directory_path,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    std::string resource_id = GetResourceIdFromPath(current);

    // Calculate MD5.
    std::string md5 = util::GetMd5Digest(current);
    if (md5.empty())
      continue;

    // Determine cache state.
    FileCacheEntry cache_entry;
    cache_entry.set_md5(md5);
    cache_entry.set_is_present(true);

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(resource_id, cache_entry));
  }
}

// Runs callback with pointers dereferenced.
// Used to implement GetFile, MarkAsMounted.
void RunGetFileFromCacheCallback(const GetFileFromCacheCallback& callback,
                                 base::FilePath* file_path,
                                 FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(file_path);

  callback.Run(error, *file_path);
}

// Runs callback with pointers dereferenced.
// Used to implement GetCacheEntry().
void RunGetCacheEntryCallback(const GetCacheEntryCallback& callback,
                              FileCacheEntry* cache_entry,
                              bool success) {
  DCHECK(cache_entry);
  DCHECK(!callback.is_null());
  callback.Run(success, *cache_entry);
}

// Calls |iteration_callback| with each entry in |cache|.
void IterateCache(FileCache* cache,
                  const CacheIterateCallback& iteration_callback) {
  scoped_ptr<FileCache::Iterator> it = cache->GetIterator();
  for (; !it->IsAtEnd(); it->Advance())
    iteration_callback.Run(it->GetID(), it->GetValue());
  DCHECK(!it->HasError());
}

}  // namespace

FileCache::FileCache(ResourceMetadataStorage* storage,
                     const base::FilePath& cache_file_directory,
                     base::SequencedTaskRunner* blocking_task_runner,
                     FreeDiskSpaceGetterInterface* free_disk_space_getter)
    : cache_file_directory_(cache_file_directory),
      blocking_task_runner_(blocking_task_runner),
      storage_(storage),
      free_disk_space_getter_(free_disk_space_getter),
      weak_ptr_factory_(this) {
  DCHECK(blocking_task_runner_.get());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FileCache::~FileCache() {
  // Must be on the sequenced worker pool, as |metadata_| must be deleted on
  // the sequenced worker pool.
  AssertOnSequencedWorkerPool();
}

base::FilePath FileCache::GetCacheFilePath(
    const std::string& resource_id) const {
  return cache_file_directory_.Append(
      base::FilePath::FromUTF8Unsafe(util::EscapeCacheFileName(resource_id)));
}

void FileCache::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_.get() ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool FileCache::IsUnderFileCacheDirectory(const base::FilePath& path) const {
  return cache_file_directory_.IsParent(path);
}

void FileCache::GetCacheEntryOnUIThread(const std::string& resource_id,
                                        const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileCacheEntry* cache_entry = new FileCacheEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(this),
                 resource_id,
                 cache_entry),
      base::Bind(
          &RunGetCacheEntryCallback, callback, base::Owned(cache_entry)));
}

bool FileCache::GetCacheEntry(const std::string& resource_id,
                              FileCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();
  return storage_->GetCacheEntry(resource_id, entry);
}

void FileCache::IterateOnUIThread(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&IterateCache,
                 base::Unretained(this),
                 google_apis::CreateRelayCallback(iteration_callback)),
      completion_callback);
}

scoped_ptr<FileCache::Iterator> FileCache::GetIterator() {
  AssertOnSequencedWorkerPool();
  return storage_->GetCacheEntryIterator();
}

bool FileCache::FreeDiskSpaceIfNeededFor(int64 num_bytes) {
  AssertOnSequencedWorkerPool();

  // Do nothing and return if we have enough space.
  if (HasEnoughSpaceFor(num_bytes, cache_file_directory_))
    return true;

  // Otherwise, try to free up the disk space.
  DVLOG(1) << "Freeing up disk space for " << num_bytes;

  // Remove all entries unless specially marked.
  scoped_ptr<ResourceMetadataStorage::CacheEntryIterator> it =
      storage_->GetCacheEntryIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const FileCacheEntry& entry = it->GetValue();
    if (!entry.is_pinned() &&
        !entry.is_dirty() &&
        !mounted_files_.count(it->GetID()))
      storage_->RemoveCacheEntry(it->GetID());
  }
  DCHECK(!it->HasError());

  // Remove all files which have no corresponding cache entries.
  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  FileCacheEntry entry;
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    std::string resource_id = GetResourceIdFromPath(current);
    if (!storage_->GetCacheEntry(resource_id, &entry))
      base::DeleteFile(current, false /* recursive */);
  }

  // Check the disk space again.
  return HasEnoughSpaceFor(num_bytes, cache_file_directory_);
}

void FileCache::GetFileOnUIThread(const std::string& resource_id,
                                  const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(blocking_task_runner_.get(),
                                   FROM_HERE,
                                   base::Bind(&FileCache::GetFile,
                                              base::Unretained(this),
                                              resource_id,
                                              cache_file_path),
                                   base::Bind(&RunGetFileFromCacheCallback,
                                              callback,
                                              base::Owned(cache_file_path)));
}

FileError FileCache::GetFile(const std::string& resource_id,
                             base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry) ||
      !cache_entry.is_present())
    return FILE_ERROR_NOT_FOUND;

  *cache_file_path = GetCacheFilePath(resource_id);
  return FILE_ERROR_OK;
}

void FileCache::StoreOnUIThread(const std::string& resource_id,
                                const std::string& md5,
                                const base::FilePath& source_path,
                                FileOperationType file_operation_type,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(blocking_task_runner_.get(),
                                   FROM_HERE,
                                   base::Bind(&FileCache::Store,
                                              base::Unretained(this),
                                              resource_id,
                                              md5,
                                              source_path,
                                              file_operation_type),
                                   callback);
}

FileError FileCache::Store(const std::string& resource_id,
                           const std::string& md5,
                           const base::FilePath& source_path,
                           FileOperationType file_operation_type) {
  AssertOnSequencedWorkerPool();
  return StoreInternal(resource_id, md5, source_path, file_operation_type);
}

void FileCache::PinOnUIThread(const std::string& resource_id,
                              const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Pin, base::Unretained(this), resource_id),
      callback);
}

FileError FileCache::Pin(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  FileCacheEntry cache_entry;
  storage_->GetCacheEntry(resource_id, &cache_entry);
  cache_entry.set_is_pinned(true);
  return storage_->PutCacheEntry(resource_id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

void FileCache::UnpinOnUIThread(const std::string& resource_id,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Unpin, base::Unretained(this), resource_id),
      callback);
}

FileError FileCache::Unpin(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  // Unpinning a file means its entry must exist in cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  // Now that file operations have completed, update metadata.
  if (cache_entry.is_present()) {
    cache_entry.set_is_pinned(false);
    if (!storage_->PutCacheEntry(resource_id, cache_entry))
      return FILE_ERROR_FAILED;
  } else {
    // Remove the existing entry if we are unpinning a non-present file.
    if  (!storage_->RemoveCacheEntry(resource_id))
      return FILE_ERROR_FAILED;
  }

  // Now it's a chance to free up space if needed.
  FreeDiskSpaceIfNeededFor(0);

  return FILE_ERROR_OK;
}

void FileCache::MarkAsMountedOnUIThread(
    const std::string& resource_id,
    const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::MarkAsMounted,
                 base::Unretained(this),
                 resource_id,
                 cache_file_path),
      base::Bind(
          RunGetFileFromCacheCallback, callback, base::Owned(cache_file_path)));
}

void FileCache::MarkAsUnmountedOnUIThread(
    const base::FilePath& file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &FileCache::MarkAsUnmounted, base::Unretained(this), file_path),
      callback);
}

void FileCache::MarkDirtyOnUIThread(const std::string& resource_id,
                                    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::MarkDirty, base::Unretained(this), resource_id),
      callback);
}

FileError FileCache::MarkDirty(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: "
                 << resource_id;
    return FILE_ERROR_NOT_FOUND;
  }

  if (cache_entry.is_dirty())
    return FILE_ERROR_OK;

  cache_entry.set_is_dirty(true);
  return storage_->PutCacheEntry(resource_id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

FileError FileCache::ClearDirty(const std::string& resource_id,
                                const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << resource_id;
    return FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: "
                 << resource_id;
    return FILE_ERROR_INVALID_OPERATION;
  }

  cache_entry.set_md5(md5);
  cache_entry.set_is_dirty(false);
  return storage_->PutCacheEntry(resource_id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

void FileCache::RemoveOnUIThread(const std::string& resource_id,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Remove, base::Unretained(this), resource_id),
      callback);
}

FileError FileCache::Remove(const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  FileCacheEntry cache_entry;

  // If entry doesn't exist, nothing to do.
  if (!storage_->GetCacheEntry(resource_id, &cache_entry))
    return FILE_ERROR_OK;

  // Cannot delete a mounted file.
  if (mounted_files_.count(resource_id))
    return FILE_ERROR_IN_USE;

  // Delete the file.
  base::FilePath path = GetCacheFilePath(resource_id);
  if (!base::DeleteFile(path, false /* recursive */))
    return FILE_ERROR_FAILED;

  // Now that all file operations have completed, remove from metadata.
  return storage_->RemoveCacheEntry(resource_id) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

void FileCache::ClearAllOnUIThread(const ClearAllCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::ClearAll, base::Unretained(this)),
      callback);
}

bool FileCache::Initialize() {
  AssertOnSequencedWorkerPool();

  RenameCacheFilesToNewFormat();

  if (!storage_->opened_existing_db()) {
    CacheMap cache_map;
    ScanCacheDirectory(cache_file_directory_, &cache_map);
    for (CacheMap::const_iterator it = cache_map.begin();
         it != cache_map.end(); ++it) {
      storage_->PutCacheEntry(it->first, it->second);
    }
  }
  return true;
}

void FileCache::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Invalidate the weak pointer.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy myself on the blocking pool.
  // Note that base::DeletePointer<> cannot be used as the destructor of this
  // class is private.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FileCache::DestroyOnBlockingPool, base::Unretained(this)));
}

void FileCache::DestroyOnBlockingPool() {
  AssertOnSequencedWorkerPool();
  delete this;
}

FileError FileCache::StoreInternal(const std::string& resource_id,
                                   const std::string& md5,
                                   const base::FilePath& source_path,
                                   FileOperationType file_operation_type) {
  AssertOnSequencedWorkerPool();

  int64 file_size = 0;
  if (file_operation_type == FILE_OPERATION_COPY) {
    if (!file_util::GetFileSize(source_path, &file_size)) {
      LOG(WARNING) << "Couldn't get file size for: " << source_path.value();
      return FILE_ERROR_FAILED;
    }
  }
  if (!FreeDiskSpaceIfNeededFor(file_size))
    return FILE_ERROR_NO_LOCAL_SPACE;

  FileCacheEntry cache_entry;
  storage_->GetCacheEntry(resource_id, &cache_entry);

  // If file is dirty or mounted, return error.
  if (cache_entry.is_dirty() || mounted_files_.count(resource_id))
    return FILE_ERROR_IN_USE;

  base::FilePath dest_path = GetCacheFilePath(resource_id);
  bool success = false;
  switch (file_operation_type) {
    case FILE_OPERATION_MOVE:
      success = base::Move(source_path, dest_path);
      break;
    case FILE_OPERATION_COPY:
      success = base::CopyFile(source_path, dest_path);
      break;
    default:
      NOTREACHED();
  }

  if (!success) {
    LOG(ERROR) << "Failed to store: "
               << "source_path = " << source_path.value() << ", "
               << "dest_path = " << dest_path.value() << ", "
               << "file_operation_type = " << file_operation_type;
    return FILE_ERROR_FAILED;
  }

  // Now that file operations have completed, update metadata.
  cache_entry.set_md5(md5);
  cache_entry.set_is_present(true);
  cache_entry.set_is_dirty(false);
  return storage_->PutCacheEntry(resource_id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

FileError FileCache::MarkAsMounted(const std::string& resource_id,
                                   base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  // Get cache entry associated with the resource_id and md5
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  if (mounted_files_.count(resource_id))
    return FILE_ERROR_INVALID_OPERATION;

  // Ensure the file is readable to cros_disks. See crbug.com/236994.
  base::FilePath path = GetCacheFilePath(resource_id);
  if (!file_util::SetPosixFilePermissions(
          path,
          file_util::FILE_PERMISSION_READ_BY_USER |
          file_util::FILE_PERMISSION_WRITE_BY_USER |
          file_util::FILE_PERMISSION_READ_BY_GROUP |
          file_util::FILE_PERMISSION_READ_BY_OTHERS))
    return FILE_ERROR_FAILED;

  mounted_files_.insert(resource_id);

  *cache_file_path = path;
  return FILE_ERROR_OK;
}

FileError FileCache::MarkAsUnmounted(const base::FilePath& file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(IsUnderFileCacheDirectory(file_path));

  std::string resource_id = GetResourceIdFromPath(file_path);

  // Get cache entry associated with the resource_id and md5
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  std::set<std::string>::iterator it = mounted_files_.find(resource_id);
  if (it == mounted_files_.end())
    return FILE_ERROR_INVALID_OPERATION;

  mounted_files_.erase(it);
  return FILE_ERROR_OK;
}

bool FileCache::ClearAll() {
  AssertOnSequencedWorkerPool();

  // Remove entries on the metadata.
  scoped_ptr<ResourceMetadataStorage::CacheEntryIterator> it =
      storage_->GetCacheEntryIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (!storage_->RemoveCacheEntry(it->GetID()))
      return false;
  }

  if (it->HasError())
    return false;

  // Remove files.
  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath file = enumerator.Next(); !file.empty();
       file = enumerator.Next())
    base::DeleteFile(file, false /* recursive */);

  return true;
}

bool FileCache::HasEnoughSpaceFor(int64 num_bytes,
                                  const base::FilePath& path) {
  int64 free_space = 0;
  if (free_disk_space_getter_)
    free_space = free_disk_space_getter_->AmountOfFreeDiskSpace();
  else
    free_space = base::SysInfo::AmountOfFreeDiskSpace(path);

  // Subtract this as if this portion does not exist.
  free_space -= kMinFreeSpace;
  return (free_space >= num_bytes);
}

void FileCache::RenameCacheFilesToNewFormat() {
  // First, remove all files with multiple extensions just in case.
  {
    base::FileEnumerator enumerator(cache_file_directory_,
                                    false,  // not recursive
                                    base::FileEnumerator::FILES,
                                    "*.*.*");
    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next())
      base::DeleteFile(current, false /* recursive */);
  }

  // Rename files.
  {
    base::FileEnumerator enumerator(cache_file_directory_,
                                    false,  // not recursive
                                    base::FileEnumerator::FILES);
    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next())
      base::Move(current, current.RemoveExtension());
  }
}

}  // namespace internal
}  // namespace drive
