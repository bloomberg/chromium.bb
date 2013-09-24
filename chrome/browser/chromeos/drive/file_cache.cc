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
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

typedef std::map<std::string, FileCacheEntry> CacheMap;

// Returns ID extracted from the path.
std::string GetIdFromPath(const base::FilePath& path) {
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
    std::string id = GetIdFromPath(current);

    // Calculate MD5.
    std::string md5 = util::GetMd5Digest(current);
    if (md5.empty())
      continue;

    // Determine cache state.
    FileCacheEntry cache_entry;
    cache_entry.set_md5(md5);
    cache_entry.set_is_present(true);

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(id, cache_entry));
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

base::FilePath FileCache::GetCacheFilePath(const std::string& id) const {
  return cache_file_directory_.Append(
      base::FilePath::FromUTF8Unsafe(util::EscapeCacheFileName(id)));
}

void FileCache::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_.get() ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool FileCache::IsUnderFileCacheDirectory(const base::FilePath& path) const {
  return cache_file_directory_.IsParent(path);
}

void FileCache::GetCacheEntryOnUIThread(const std::string& id,
                                        const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileCacheEntry* cache_entry = new FileCacheEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(this),
                 id,
                 cache_entry),
      base::Bind(
          &RunGetCacheEntryCallback, callback, base::Owned(cache_entry)));
}

bool FileCache::GetCacheEntry(const std::string& id, FileCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();
  return storage_->GetCacheEntry(id, entry);
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
    std::string id = GetIdFromPath(current);
    if (!storage_->GetCacheEntry(id, &entry))
      base::DeleteFile(current, false /* recursive */);
  }

  // Check the disk space again.
  return HasEnoughSpaceFor(num_bytes, cache_file_directory_);
}

FileError FileCache::GetFile(const std::string& id,
                             base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry) ||
      !cache_entry.is_present())
    return FILE_ERROR_NOT_FOUND;

  *cache_file_path = GetCacheFilePath(id);
  return FILE_ERROR_OK;
}

void FileCache::StoreOnUIThread(const std::string& id,
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
                                              id,
                                              md5,
                                              source_path,
                                              file_operation_type),
                                   callback);
}

FileError FileCache::Store(const std::string& id,
                           const std::string& md5,
                           const base::FilePath& source_path,
                           FileOperationType file_operation_type) {
  AssertOnSequencedWorkerPool();
  return StoreInternal(id, md5, source_path, file_operation_type);
}

void FileCache::PinOnUIThread(const std::string& id,
                              const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Pin, base::Unretained(this), id),
      callback);
}

FileError FileCache::Pin(const std::string& id) {
  AssertOnSequencedWorkerPool();

  FileCacheEntry cache_entry;
  storage_->GetCacheEntry(id, &cache_entry);
  cache_entry.set_is_pinned(true);
  return storage_->PutCacheEntry(id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

void FileCache::UnpinOnUIThread(const std::string& id,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Unpin, base::Unretained(this), id),
      callback);
}

FileError FileCache::Unpin(const std::string& id) {
  AssertOnSequencedWorkerPool();

  // Unpinning a file means its entry must exist in cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  // Now that file operations have completed, update metadata.
  if (cache_entry.is_present()) {
    cache_entry.set_is_pinned(false);
    if (!storage_->PutCacheEntry(id, cache_entry))
      return FILE_ERROR_FAILED;
  } else {
    // Remove the existing entry if we are unpinning a non-present file.
    if  (!storage_->RemoveCacheEntry(id))
      return FILE_ERROR_FAILED;
  }

  // Now it's a chance to free up space if needed.
  FreeDiskSpaceIfNeededFor(0);

  return FILE_ERROR_OK;
}

void FileCache::MarkAsMountedOnUIThread(
    const std::string& id,
    const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::MarkAsMounted,
                 base::Unretained(this),
                 id,
                 cache_file_path),
      base::Bind(
          RunGetFileFromCacheCallback, callback, base::Owned(cache_file_path)));
}

FileError FileCache::MarkAsMounted(const std::string& id,
                                   base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  // Get cache entry associated with the id and md5
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  if (mounted_files_.count(id))
    return FILE_ERROR_INVALID_OPERATION;

  // Ensure the file is readable to cros_disks. See crbug.com/236994.
  base::FilePath path = GetCacheFilePath(id);
  if (!file_util::SetPosixFilePermissions(
          path,
          file_util::FILE_PERMISSION_READ_BY_USER |
          file_util::FILE_PERMISSION_WRITE_BY_USER |
          file_util::FILE_PERMISSION_READ_BY_GROUP |
          file_util::FILE_PERMISSION_READ_BY_OTHERS))
    return FILE_ERROR_FAILED;

  mounted_files_.insert(id);

  *cache_file_path = path;
  return FILE_ERROR_OK;
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

void FileCache::MarkDirtyOnUIThread(const std::string& id,
                                    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::MarkDirty, base::Unretained(this), id),
      callback);
}

FileError FileCache::MarkDirty(const std::string& id) {
  AssertOnSequencedWorkerPool();

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: " << id;
    return FILE_ERROR_NOT_FOUND;
  }

  if (cache_entry.is_dirty())
    return FILE_ERROR_OK;

  cache_entry.set_is_dirty(true);
  return storage_->PutCacheEntry(id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

FileError FileCache::ClearDirty(const std::string& id, const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << id;
    return FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: " << id;
    return FILE_ERROR_INVALID_OPERATION;
  }

  cache_entry.set_md5(md5);
  cache_entry.set_is_dirty(false);
  return storage_->PutCacheEntry(id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

void FileCache::RemoveOnUIThread(const std::string& id,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::Remove, base::Unretained(this), id),
      callback);
}

FileError FileCache::Remove(const std::string& id) {
  AssertOnSequencedWorkerPool();

  FileCacheEntry cache_entry;

  // If entry doesn't exist, nothing to do.
  if (!storage_->GetCacheEntry(id, &cache_entry))
    return FILE_ERROR_OK;

  // Cannot delete a mounted file.
  if (mounted_files_.count(id))
    return FILE_ERROR_IN_USE;

  // Delete the file.
  base::FilePath path = GetCacheFilePath(id);
  if (!base::DeleteFile(path, false /* recursive */))
    return FILE_ERROR_FAILED;

  // Now that all file operations have completed, remove from metadata.
  return storage_->RemoveCacheEntry(id) ? FILE_ERROR_OK : FILE_ERROR_FAILED;
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

bool FileCache::Initialize() {
  AssertOnSequencedWorkerPool();

  RenameCacheFilesToNewFormat();

  if (storage_->cache_file_scan_is_needed()) {
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

bool FileCache::CanonicalizeIDs(
    const ResourceIdCanonicalizer& id_canonicalizer) {
  scoped_ptr<Iterator> it = GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const std::string id_canonicalized = id_canonicalizer.Run(it->GetID());
    if (id_canonicalized != it->GetID()) {
      // Replace the existing entry and rename the file when needed.
      const base::FilePath path_old = GetCacheFilePath(it->GetID());
      const base::FilePath path_new = GetCacheFilePath(id_canonicalized);
      if (!storage_->RemoveCacheEntry(it->GetID()) ||
          (base::PathExists(path_old) && !base::Move(path_old, path_new)) ||
          !storage_->PutCacheEntry(id_canonicalized, it->GetValue()))
        return false;
    }
  }
  return !it->HasError();
}

void FileCache::DestroyOnBlockingPool() {
  AssertOnSequencedWorkerPool();
  delete this;
}

FileError FileCache::StoreInternal(const std::string& id,
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
  storage_->GetCacheEntry(id, &cache_entry);

  // If file is dirty or mounted, return error.
  if (cache_entry.is_dirty() || mounted_files_.count(id))
    return FILE_ERROR_IN_USE;

  base::FilePath dest_path = GetCacheFilePath(id);
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
  return storage_->PutCacheEntry(id, cache_entry) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

FileError FileCache::MarkAsUnmounted(const base::FilePath& file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(IsUnderFileCacheDirectory(file_path));

  std::string id = GetIdFromPath(file_path);

  // Get cache entry associated with the id and md5
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  std::set<std::string>::iterator it = mounted_files_.find(id);
  if (it == mounted_files_.end())
    return FILE_ERROR_INVALID_OPERATION;

  mounted_files_.erase(it);
  return FILE_ERROR_OK;
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
