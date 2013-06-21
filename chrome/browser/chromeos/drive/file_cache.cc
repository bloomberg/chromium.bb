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
#include "chrome/browser/chromeos/drive/file_cache_metadata.h"
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

// Returns true if |md5| matches the one in |cache_entry| with some
// exceptions. See the function definition for details.
bool CheckIfMd5Matches(const std::string& md5,
                       const FileCacheEntry& cache_entry) {
  if (cache_entry.is_dirty()) {
    // If the entry is dirty, its MD5 may have been replaced by "local"
    // during cache initialization, so we don't compare MD5.
    return true;
  } else if (cache_entry.is_pinned() && cache_entry.md5().empty()) {
    // If the entry is pinned, it's ok for the entry to have an empty
    // MD5. This can happen if the pinned file is not fetched.
    return true;
  } else if (md5.empty()) {
    // If the MD5 matching is not requested, don't check MD5.
    return true;
  } else if (md5 == cache_entry.md5()) {
    // Otherwise, compare the MD5.
    return true;
  }
  return false;
}

// Scans cache subdirectory and insert found files to |cache_map|.
void ScanCacheDirectory(const base::FilePath& directory_path,
                        CacheMap* cache_map) {
  base::FileEnumerator enumerator(directory_path,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Extract resource_id and md5 from filename.
    std::string resource_id;
    std::string md5;
    util::ParseCacheFilePath(current, &resource_id, &md5);

    // Determine cache state.
    FileCacheEntry cache_entry;
    cache_entry.set_md5(md5);
    cache_entry.set_is_present(true);

    // Add the dirty bit if |md5| indicates that the file is dirty.
    if (md5 == util::kLocallyModifiedFileExtension)
      cache_entry.set_is_dirty(true);

    // Create and insert new entry into cache map.
    cache_map->insert(std::make_pair(resource_id, cache_entry));
  }
}

// Moves the file.
bool MoveFile(const base::FilePath& source_path,
              const base::FilePath& dest_path) {
  if (!file_util::Move(source_path, dest_path)) {
    LOG(ERROR) << "Failed to move " << source_path.value()
               << " to " << dest_path.value();
    return false;
  }
  DVLOG(1) << "Moved " << source_path.value() << " to " << dest_path.value();
  return true;
}

// Copies the file.
bool CopyFile(const base::FilePath& source_path,
              const base::FilePath& dest_path) {
  if (!file_util::CopyFile(source_path, dest_path)) {
    LOG(ERROR) << "Failed to copy " << source_path.value()
               << " to " << dest_path.value();
    return false;
  }
  DVLOG(1) << "Copied " << source_path.value() << " to " << dest_path.value();
  return true;
}

// Deletes all files that match |path_to_delete_pattern| except for
// |path_to_keep| on blocking pool.
// If |path_to_keep| is empty, all files in |path_to_delete_pattern| are
// deleted.
void DeleteFilesSelectively(const base::FilePath& path_to_delete_pattern,
                            const base::FilePath& path_to_keep) {
  // Enumerate all files in directory of |path_to_delete_pattern| that match
  // base name of |path_to_delete_pattern|.
  // If a file is not |path_to_keep|, delete it.
  bool success = true;
  base::FileEnumerator enumerator(
      path_to_delete_pattern.DirName(),
      false,  // not recursive
      base::FileEnumerator::FILES,
      path_to_delete_pattern.BaseName().value());
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // If |path_to_keep| is not empty and same as current, don't delete it.
    if (!path_to_keep.empty() && current == path_to_keep)
      continue;

    success = file_util::Delete(current, false);
    if (!success)
      DVLOG(1) << "Error deleting " << current.value();
    else
      DVLOG(1) << "Deleted " << current.value();
  }
}

// Runs callback with pointers dereferenced.
// Used to implement GetFile, MarkAsMounted.
void RunGetFileFromCacheCallback(
    const GetFileFromCacheCallback& callback,
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

const base::FilePath::CharType FileCache::kOldCacheMetadataDBName[] =
    FILE_PATH_LITERAL("cache_metadata.db");

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

base::FilePath FileCache::GetCacheFilePath(const std::string& resource_id,
                                           const std::string& md5,
                                           CachedFileOrigin file_origin) const {
  // Runs on any thread.
  // Filename is formatted as resource_id.md5, i.e. resource_id is the base
  // name and md5 is the extension.
  std::string base_name = util::EscapeCacheFileName(resource_id);
  if (file_origin == CACHED_FILE_LOCALLY_MODIFIED) {
    base_name += base::FilePath::kExtensionSeparator;
    base_name += util::kLocallyModifiedFileExtension;
  } else if (!md5.empty()) {
    base_name += base::FilePath::kExtensionSeparator;
    base_name += util::EscapeCacheFileName(md5);
  }
  return cache_file_directory_.Append(
      base::FilePath::FromUTF8Unsafe(base_name));
}

void FileCache::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_.get() ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool FileCache::IsUnderFileCacheDirectory(const base::FilePath& path) const {
  return cache_file_directory_.IsParent(path);
}

void FileCache::GetCacheEntryOnUIThread(const std::string& resource_id,
                                        const std::string& md5,
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
                 md5,
                 cache_entry),
      base::Bind(
          &RunGetCacheEntryCallback, callback, base::Owned(cache_entry)));
}

bool FileCache::GetCacheEntry(const std::string& resource_id,
                              const std::string& md5,
                              FileCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();
  return storage_->GetCacheEntry(resource_id, entry) &&
      CheckIfMd5Matches(md5, *entry);
}

void FileCache::IterateOnUIThread(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FileCache::Iterate,
                 base::Unretained(this),
                 google_apis::CreateRelayCallback(iteration_callback)),
      completion_callback);
}

void FileCache::Iterate(const CacheIterateCallback& iteration_callback) {
  AssertOnSequencedWorkerPool();
  DCHECK(!iteration_callback.is_null());

  scoped_ptr<ResourceMetadataStorage::CacheEntryIterator> it =
      storage_->GetCacheEntryIterator();
  for (; !it->IsAtEnd(); it->Advance())
    iteration_callback.Run(it->GetID(), it->GetValue());
  DCHECK(!it->HasError());
}

void FileCache::FreeDiskSpaceIfNeededForOnUIThread(
    int64 num_bytes,
    const InitializeCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileCache::FreeDiskSpaceIfNeededFor,
                 base::Unretained(this),
                 num_bytes),
      callback);
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
  std::string resource_id;
  std::string md5;
  FileCacheEntry entry;
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    util::ParseCacheFilePath(current, &resource_id, &md5);
    if (!GetCacheEntry(resource_id, md5, &entry))
      file_util::Delete(current, false /* recursive */);
  }

  // Check the disk space again.
  return HasEnoughSpaceFor(num_bytes, cache_file_directory_);
}

void FileCache::GetFileOnUIThread(const std::string& resource_id,
                                  const std::string& md5,
                                  const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(blocking_task_runner_.get(),
                                   FROM_HERE,
                                   base::Bind(&FileCache::GetFile,
                                              base::Unretained(this),
                                              resource_id,
                                              md5,
                                              cache_file_path),
                                   base::Bind(&RunGetFileFromCacheCallback,
                                              callback,
                                              base::Owned(cache_file_path)));
}

FileError FileCache::GetFile(const std::string& resource_id,
                             const std::string& md5,
                             base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  FileCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, md5, &cache_entry) ||
      !cache_entry.is_present())
    return FILE_ERROR_NOT_FOUND;

  CachedFileOrigin file_origin = cache_entry.is_dirty() ?
      CACHED_FILE_LOCALLY_MODIFIED : CACHED_FILE_FROM_SERVER;
  *cache_file_path = GetCacheFilePath(resource_id, cache_entry.md5(),
                                      file_origin);
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
  storage_->PutCacheEntry(resource_id, cache_entry);
  return FILE_ERROR_OK;
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
    storage_->PutCacheEntry(resource_id, cache_entry);
  } else {
    // Remove the existing entry if we are unpinning a non-present file.
    storage_->RemoveCacheEntry(resource_id);
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
                                    const std::string& md5,
                                    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &FileCache::MarkDirty, base::Unretained(this), resource_id, md5),
      callback);
}

FileError FileCache::MarkDirty(const std::string& resource_id,
                               const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  FileCacheEntry cache_entry;
  if (!storage_->GetCacheEntry(resource_id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return FILE_ERROR_NOT_FOUND;
  }

  if (cache_entry.is_dirty())
    return FILE_ERROR_OK;

  // Get the current path of the file in cache.
  base::FilePath source_path = GetCacheFilePath(resource_id, md5,
                                                CACHED_FILE_FROM_SERVER);
  // Determine destination path.
  base::FilePath cache_file_path = GetCacheFilePath(
      resource_id, md5, CACHED_FILE_LOCALLY_MODIFIED);

  if (!MoveFile(source_path, cache_file_path))
    return FILE_ERROR_FAILED;

  // Now that file operations have completed, update metadata.
  cache_entry.set_md5(md5);
  cache_entry.set_is_dirty(true);
  storage_->PutCacheEntry(resource_id, cache_entry);
  return FILE_ERROR_OK;
}

FileError FileCache::ClearDirty(const std::string& resource_id,
                                const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // |md5| is the new .<md5> extension to rename the file to.
  // So, search for entry in cache without comparing md5.
  FileCacheEntry cache_entry;

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  if (!storage_->GetCacheEntry(resource_id, &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << "res_id=" << resource_id
                 << ", md5=" << md5;
    return FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return FILE_ERROR_INVALID_OPERATION;
  }

  base::FilePath source_path = GetCacheFilePath(resource_id, md5,
                                                CACHED_FILE_LOCALLY_MODIFIED);
  base::FilePath dest_path = GetCacheFilePath(resource_id, md5,
                                              CACHED_FILE_FROM_SERVER);
  if (!MoveFile(source_path, dest_path))
    return FILE_ERROR_FAILED;

  // Now that file operations have completed, update metadata.
  cache_entry.set_md5(md5);
  cache_entry.set_is_dirty(false);
  storage_->PutCacheEntry(resource_id, cache_entry);
  return FILE_ERROR_OK;
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

  // MD5 is not passed into RemoveCacheEntry because we would delete all
  // cache files corresponding to <resource_id> regardless of the md5.
  // So, search for entry in cache without taking md5 into account.
  FileCacheEntry cache_entry;

  // If entry doesn't exist, nothing to do.
  if (!storage_->GetCacheEntry(resource_id, &cache_entry))
    return FILE_ERROR_OK;

  // Cannot delete a dirty or mounted file.
  if (cache_entry.is_dirty() || mounted_files_.count(resource_id))
    return FILE_ERROR_IN_USE;

  // Delete files that match "<resource_id>.*" unless modified locally.
  base::FilePath path_to_delete = GetCacheFilePath(resource_id, util::kWildCard,
                                                   CACHED_FILE_FROM_SERVER);
  base::FilePath path_to_keep = GetCacheFilePath(resource_id, std::string(),
                                                 CACHED_FILE_LOCALLY_MODIFIED);
  DeleteFilesSelectively(path_to_delete, path_to_keep);

  // Now that all file operations have completed, remove from metadata.
  storage_->RemoveCacheEntry(resource_id);

  return FILE_ERROR_OK;
}

void FileCache::ClearAllOnUIThread(const InitializeCacheCallback& callback) {
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

  if (!ImportOldDB(storage_->directory_path().Append(
          kOldCacheMetadataDBName)) &&
      !storage_->opened_existing_db()) {
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
    return FILE_ERROR_NO_SPACE;

  FileCacheEntry cache_entry;
  storage_->GetCacheEntry(resource_id, &cache_entry);

  // If file is dirty or mounted, return error.
  if (cache_entry.is_dirty() || mounted_files_.count(resource_id))
    return FILE_ERROR_IN_USE;

  base::FilePath dest_path = GetCacheFilePath(resource_id, md5,
                                              CACHED_FILE_FROM_SERVER);
  bool success = false;
  switch (file_operation_type) {
    case FILE_OPERATION_MOVE:
      success = MoveFile(source_path, dest_path);
      break;
    case FILE_OPERATION_COPY:
      success = CopyFile(source_path, dest_path);
      break;
    default:
      NOTREACHED();
  }

  // Determine search pattern for stale filenames corresponding to resource_id,
  // either "<resource_id>*" or "<resource_id>.*".
  base::FilePath stale_filenames_pattern;
  if (md5.empty()) {
    // No md5 means no extension, append '*' after base name, i.e.
    // "<resource_id>*".
    // Cannot call |dest_path|.ReplaceExtension when there's no md5 extension:
    // if base name of |dest_path| (i.e. escaped resource_id) contains the
    // extension separator '.', ReplaceExtension will remove it and everything
    // after it.  The result will be nothing like the escaped resource_id.
    stale_filenames_pattern =
        base::FilePath(dest_path.value() + util::kWildCard);
  } else {
    // Replace md5 extension with '*' i.e. "<resource_id>.*".
    // Note that ReplaceExtension automatically prefixes the extension with the
    // extension separator '.'.
    stale_filenames_pattern = dest_path.ReplaceExtension(util::kWildCard);
  }

  // Delete files that match |stale_filenames_pattern| except for |dest_path|.
  DeleteFilesSelectively(stale_filenames_pattern, dest_path);

  if (success) {
    // Now that file operations have completed, update metadata.
    cache_entry.set_md5(md5);
    cache_entry.set_is_present(true);
    cache_entry.set_is_dirty(false);
    storage_->PutCacheEntry(resource_id, cache_entry);
  }

  return success ? FILE_ERROR_OK : FILE_ERROR_FAILED;
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
  base::FilePath path = GetCacheFilePath(
      resource_id, cache_entry.md5(), CACHED_FILE_FROM_SERVER);
  file_util::SetPosixFilePermissions(
      path,
      file_util::FILE_PERMISSION_READ_BY_USER |
      file_util::FILE_PERMISSION_WRITE_BY_USER |
      file_util::FILE_PERMISSION_READ_BY_GROUP |
      file_util::FILE_PERMISSION_READ_BY_OTHERS);

  mounted_files_.insert(resource_id);

  *cache_file_path = path;
  return FILE_ERROR_OK;
}

FileError FileCache::MarkAsUnmounted(const base::FilePath& file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(IsUnderFileCacheDirectory(file_path));

  // Parse file path to obtain resource_id, md5 and extra_extension.
  std::string resource_id;
  std::string md5;
  util::ParseCacheFilePath(file_path, &resource_id, &md5);

  // Get cache entry associated with the resource_id and md5
  FileCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, md5, &cache_entry))
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
  for (; !it->IsAtEnd(); it->Advance())
    storage_->RemoveCacheEntry(it->GetID());

  if (it->HasError())
    return false;

  // Remove files.
  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath file = enumerator.Next(); !file.empty();
       file = enumerator.Next())
    file_util::Delete(file, false /* recursive */);

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

bool FileCache::ImportOldDB(const base::FilePath& old_db_path) {
  if (!file_util::PathExists(old_db_path))  // Old DB is not there, do nothing.
    return false;

  // Copy all entries stored in the old DB.
  bool imported = false;
  {
    FileCacheMetadata old_data(blocking_task_runner_);
    if (old_data.Initialize(old_db_path) ==
        FileCacheMetadata::INITIALIZE_OPENED) {
      scoped_ptr<FileCacheMetadata::Iterator> it = old_data.GetIterator();
      for (; !it->IsAtEnd(); it->Advance()) {
        FileCacheEntry entry;
        if (storage_->GetCacheEntry(it->GetKey(), &entry))
          continue;  // Do not overwrite.

        storage_->PutCacheEntry(it->GetKey(), it->GetValue());
      }
      imported = true;
    }
  }

  // Delete old DB.
  file_util::Delete(old_db_path, true /* recursive */ );
  return imported;
}

}  // namespace internal
}  // namespace drive
