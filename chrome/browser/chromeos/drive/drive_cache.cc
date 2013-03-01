// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_cache.h"

#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache_metadata.h"
#include "chrome/browser/chromeos/drive/drive_cache_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

const base::FilePath::CharType kDriveCacheVersionDir[] =
    FILE_PATH_LITERAL("v1");
const base::FilePath::CharType kDriveCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const base::FilePath::CharType kDriveCachePinnedDir[] =
    FILE_PATH_LITERAL("pinned");
const base::FilePath::CharType kDriveCacheOutgoingDir[] =
    FILE_PATH_LITERAL("outgoing");
const base::FilePath::CharType kDriveCachePersistentDir[] =
    FILE_PATH_LITERAL("persistent");
const base::FilePath::CharType kDriveCacheTmpDir[] = FILE_PATH_LITERAL("tmp");
const base::FilePath::CharType kDriveCacheTmpDownloadsDir[] =
    FILE_PATH_LITERAL("tmp/downloads");
const base::FilePath::CharType kDriveCacheTmpDocumentsDir[] =
    FILE_PATH_LITERAL("tmp/documents");

// Create cache directory paths and set permissions.
bool InitCachePaths(const std::vector<base::FilePath>& cache_paths) {
  if (cache_paths.size() < DriveCache::NUM_CACHE_TYPES) {
    NOTREACHED();
    LOG(ERROR) << "Size of cache_paths is invalid.";
    return false;
  }

  if (!DriveCache::CreateCacheDirectories(cache_paths))
    return false;

  // Change permissions of cache persistent directory to u+rwx,og+x (711) in
  // order to allow archive files in that directory to be mounted by cros-disks.
  file_util::SetPosixFilePermissions(
      cache_paths[DriveCache::CACHE_TYPE_PERSISTENT],
      file_util::FILE_PERMISSION_USER_MASK |
      file_util::FILE_PERMISSION_EXECUTE_BY_GROUP |
      file_util::FILE_PERMISSION_EXECUTE_BY_OTHERS);

  return true;
}

// Remove all files under the given directory, non-recursively.
// Do not remove recursively as we don't want to touch <gcache>/tmp/downloads,
// which is used for user initiated downloads like "Save As"
void RemoveAllFiles(const base::FilePath& directory) {
  using file_util::FileEnumerator;

  FileEnumerator enumerator(directory, false /* recursive */,
                            FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    DVLOG(1) << "Removing " << file_path.value();
    if (!file_util::Delete(file_path, false /* recursive */))
      LOG(WARNING) << "Failed to delete " << file_path.value();
  }
}

// Deletes the symlink.
void DeleteSymlink(const base::FilePath& symlink_path) {
  // We try to save one file operation by not checking if link exists before
  // deleting it, so unlink may return error if link doesn't exist, but it
  // doesn't really matter to us.
  file_util::Delete(symlink_path, false);
}

// Creates a symlink.
bool CreateSymlink(const base::FilePath& cache_file_path,
                   const base::FilePath& symlink_path) {
  // Remove symlink because creating a link will not overwrite an existing one.
  DeleteSymlink(symlink_path);

  // Create new symlink to |cache_file_path|.
  if (!file_util::CreateSymbolicLink(cache_file_path, symlink_path)) {
    LOG(ERROR) << "Failed to create a symlink from " << symlink_path.value()
               << " to " << cache_file_path.value();
    return false;
  }
  return true;
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
  file_util::FileEnumerator enumerator(path_to_delete_pattern.DirName(),
      false,  // not recursive
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::SHOW_SYM_LINKS,
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
    scoped_ptr<std::pair<DriveFileError, base::FilePath> > result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  callback.Run(result->first, result->second);
}

// Runs callback with pointers dereferenced.
// Used to implement GetCacheEntry().
void RunGetCacheEntryCallback(const GetCacheEntryCallback& callback,
                              DriveCacheEntry* cache_entry,
                              bool success) {
  DCHECK(cache_entry);
  DCHECK(!callback.is_null());
  callback.Run(success, *cache_entry);
}

}  // namespace

DriveCache::DriveCache(const base::FilePath& cache_root_path,
                       base::SequencedTaskRunner* blocking_task_runner,
                       FreeDiskSpaceGetterInterface* free_disk_space_getter)
    : cache_root_path_(cache_root_path),
      cache_paths_(GetCachePaths(cache_root_path_)),
      blocking_task_runner_(blocking_task_runner),
      free_disk_space_getter_(free_disk_space_getter),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(blocking_task_runner_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveCache::~DriveCache() {
  // Must be on the sequenced worker pool, as |metadata_| must be deleted on
  // the sequenced worker pool.
  AssertOnSequencedWorkerPool();
}

base::FilePath DriveCache::GetCacheDirectoryPath(
    CacheSubDirectoryType sub_dir_type) const {
  DCHECK_LE(0, sub_dir_type);
  DCHECK_GT(NUM_CACHE_TYPES, sub_dir_type);
  return cache_paths_[sub_dir_type];
}

base::FilePath DriveCache::GetCacheFilePath(
    const std::string& resource_id,
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
    base_name += base::FilePath::kExtensionSeparator;
    base_name += util::kLocallyModifiedFileExtension;
  } else if (!md5.empty()) {
    base_name += base::FilePath::kExtensionSeparator;
    base_name += util::EscapeCacheFileName(md5);
  }
  // For mounted archives the filename is formatted as resource_id.md5.mounted,
  // i.e. resource_id.md5 is the base name and ".mounted" is the extension
  if (file_origin == CACHED_FILE_MOUNTED) {
    DCHECK(sub_dir_type == CACHE_TYPE_PERSISTENT);
    base_name += base::FilePath::kExtensionSeparator;
    base_name += util::kMountedArchiveFileExtension;
  }
  return GetCacheDirectoryPath(sub_dir_type).Append(base_name);
}

void DriveCache::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_ ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool DriveCache::IsUnderDriveCacheDirectory(const base::FilePath& path) const {
  return cache_root_path_ == path || cache_root_path_.IsParent(path);
}

void DriveCache::AddObserver(DriveCacheObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DriveCache::RemoveObserver(DriveCacheObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DriveCache::GetCacheEntry(const std::string& resource_id,
                               const std::string& md5,
                               const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveCacheEntry* cache_entry = new DriveCacheEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::GetCacheEntryOnBlockingPool,
                 base::Unretained(this), resource_id, md5, cache_entry),
      base::Bind(&RunGetCacheEntryCallback,
                 callback, base::Owned(cache_entry)));
}

void DriveCache::Iterate(const CacheIterateCallback& iteration_callback,
                         const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DriveCache::IterateOnBlockingPool,
                 base::Unretained(this),
                 google_apis::CreateRelayCallback(iteration_callback)),
      completion_callback);
}

void DriveCache::FreeDiskSpaceIfNeededFor(
    int64 num_bytes,
    const InitializeCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::FreeDiskSpaceOnBlockingPoolIfNeededFor,
                 base::Unretained(this),
                 num_bytes),
      callback);
}

void DriveCache::GetFile(const std::string& resource_id,
                         const std::string& md5,
                         const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::GetFileOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      base::Bind(&RunGetFileFromCacheCallback, callback));
}

void DriveCache::Store(const std::string& resource_id,
                       const std::string& md5,
                       const base::FilePath& source_path,
                       FileOperationType file_operation_type,
                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::StoreOnBlockingPool,
                 base::Unretained(this),
                 resource_id, md5, source_path, file_operation_type),
      callback);
}

void DriveCache::Pin(const std::string& resource_id,
                     const std::string& md5,
                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::PinOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      base::Bind(&DriveCache::OnPinned,
                 weak_ptr_factory_.GetWeakPtr(), resource_id, md5, callback));
}

void DriveCache::Unpin(const std::string& resource_id,
                       const std::string& md5,
                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::UnpinOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      base::Bind(&DriveCache::OnUnpinned,
                 weak_ptr_factory_.GetWeakPtr(), resource_id, md5, callback));
}

void DriveCache::MarkAsMounted(const std::string& resource_id,
                               const std::string& md5,
                               const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::MarkAsMountedOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      base::Bind(RunGetFileFromCacheCallback, callback));
}

void DriveCache::MarkAsUnmounted(const base::FilePath& file_path,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::MarkAsUnmountedOnBlockingPool,
                 base::Unretained(this), file_path),
      callback);
}

void DriveCache::MarkDirty(const std::string& resource_id,
                           const std::string& md5,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::MarkDirtyOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      callback);
}

void DriveCache::CommitDirty(const std::string& resource_id,
                             const std::string& md5,
                             const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::CommitDirtyOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      base::Bind(&DriveCache::OnCommitDirty,
                 weak_ptr_factory_.GetWeakPtr(), resource_id, callback));
}

void DriveCache::ClearDirty(const std::string& resource_id,
                            const std::string& md5,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::ClearDirtyOnBlockingPool,
                 base::Unretained(this), resource_id, md5),
      callback);
}

void DriveCache::Remove(const std::string& resource_id,
                        const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::RemoveOnBlockingPool,
                 base::Unretained(this), resource_id),
      callback);
}

void DriveCache::ClearAll(const InitializeCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::ClearAllOnBlockingPool, base::Unretained(this)),
      callback);
}

void DriveCache::RequestInitialize(const InitializeCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&DriveCache::InitializeOnBlockingPool, base::Unretained(this)),
      callback);
}

void DriveCache::RequestInitializeForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DriveCache::InitializeOnBlockingPoolForTesting,
                 base::Unretained(this)));
}

void DriveCache::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Invalidate the weak pointer.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy myself on the blocking pool.
  // Note that base::DeletePointer<> cannot be used as the destructor of this
  // class is private.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DriveCache::DestroyOnBlockingPool, base::Unretained(this)));
}

bool DriveCache::InitializeOnBlockingPool() {
  AssertOnSequencedWorkerPool();

  if (!InitCachePaths(cache_paths_))
    return false;

  metadata_ = DriveCacheMetadata::CreateDriveCacheMetadata(
      blocking_task_runner_);
  return metadata_->Initialize(cache_paths_);
}

void DriveCache::InitializeOnBlockingPoolForTesting() {
  AssertOnSequencedWorkerPool();

  InitCachePaths(cache_paths_);
  metadata_ = DriveCacheMetadata::CreateDriveCacheMetadataForTesting(
      blocking_task_runner_);
  metadata_->Initialize(cache_paths_);
}

void DriveCache::DestroyOnBlockingPool() {
  AssertOnSequencedWorkerPool();
  delete this;
}

bool DriveCache::GetCacheEntryOnBlockingPool(const std::string& resource_id,
                                             const std::string& md5,
                                             DriveCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();
  return metadata_->GetCacheEntry(resource_id, md5, entry);
}

void DriveCache::IterateOnBlockingPool(
    const CacheIterateCallback& iteration_callback) {
  AssertOnSequencedWorkerPool();
  DCHECK(!iteration_callback.is_null());

  metadata_->Iterate(iteration_callback);
}

bool DriveCache::FreeDiskSpaceOnBlockingPoolIfNeededFor(int64 num_bytes) {
  AssertOnSequencedWorkerPool();

  // Do nothing and return if we have enough space.
  if (HasEnoughSpaceFor(num_bytes, cache_root_path_))
    return true;

  // Otherwise, try to free up the disk space.
  DVLOG(1) << "Freeing up disk space for " << num_bytes;
  // First remove temporary files from the metadata.
  metadata_->RemoveTemporaryFiles();
  // Then remove all files under "tmp" directory.
  RemoveAllFiles(GetCacheDirectoryPath(CACHE_TYPE_TMP));

  // Check the disk space again.
  return HasEnoughSpaceFor(num_bytes, cache_root_path_);
}

scoped_ptr<DriveCache::GetFileResult> DriveCache::GetFileOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  scoped_ptr<GetFileResult> result(new GetFileResult);

  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry) ||
      !cache_entry.is_present()) {
    result->first = DRIVE_FILE_ERROR_NOT_FOUND;
    return result.Pass();
  }

  CachedFileOrigin file_origin;
  if (cache_entry.is_mounted()) {
    file_origin = CACHED_FILE_MOUNTED;
  } else if (cache_entry.is_dirty()) {
    file_origin = CACHED_FILE_LOCALLY_MODIFIED;
  } else {
    file_origin = CACHED_FILE_FROM_SERVER;
  }
  result->first = DRIVE_FILE_OK;
  result->second = GetCacheFilePath(resource_id,
                                    md5,
                                    GetSubDirectoryType(cache_entry),
                                    file_origin);
  return result.Pass();
}

DriveFileError DriveCache::StoreOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5,
    const base::FilePath& source_path,
    FileOperationType file_operation_type) {
  AssertOnSequencedWorkerPool();

  if (file_operation_type == FILE_OPERATION_COPY) {
    int64 file_size;
    if (!file_util::GetFileSize(source_path, &file_size)) {
      LOG(WARNING) << "Couldn't get file size for: " << source_path.value();
      return DRIVE_FILE_ERROR_FAILED;
    }

    const bool enough_space = FreeDiskSpaceOnBlockingPoolIfNeededFor(file_size);
    if (!enough_space)
      return DRIVE_FILE_ERROR_NO_SPACE;
  }

  base::FilePath symlink_path;
  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_TMP;

  // If file was previously pinned, store it in persistent dir.
  DriveCacheEntry cache_entry;
  if (GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry)) {
    // File exists in cache.
    // If file is dirty or mounted, return error.
    if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
      LOG(WARNING) << "Can't store a file to replace a "
                   << (cache_entry.is_dirty() ? "dirty" : "mounted")
                   << " file: res_id=" << resource_id
                   << ", md5=" << md5;
      return DRIVE_FILE_ERROR_IN_USE;
    }

    if (cache_entry.is_pinned())
      sub_dir_type = CACHE_TYPE_PERSISTENT;
  }

  base::FilePath dest_path = GetCacheFilePath(resource_id, md5, sub_dir_type,
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

  // Create symlink in pinned directory if the file is pinned.
  if (success && cache_entry.is_pinned()) {
    base::FilePath symlink_path = GetCacheFilePath(resource_id, std::string(),
                                             CACHE_TYPE_PINNED,
                                             CACHED_FILE_FROM_SERVER);
    success = CreateSymlink(dest_path, symlink_path);
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
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }

  return success ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
}

DriveFileError DriveCache::PinOnBlockingPool(const std::string& resource_id,
                                             const std::string& md5) {
  AssertOnSequencedWorkerPool();

  base::FilePath dest_path;
  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_PERSISTENT;

  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry)) {
    // Entry does not exist in cache. Set |dest_path| to /dev/null, so that
    // symlinks to /dev/null will be picked up by DriveSyncClient to download
    // pinned files that don't exist in cache.
    dest_path = base::FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull);

    // Set sub_dir_type to TMP. The file will be first downloaded in 'tmp',
    // then moved to 'persistent'.
    sub_dir_type = CACHE_TYPE_TMP;
  } else {  // File exists in cache, determines destination path.
    // Determine source and destination paths.

    // If file is dirty or mounted, don't move it.
    if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
      DCHECK(cache_entry.is_persistent());
      dest_path = GetCacheFilePath(resource_id,
                                   md5,
                                   GetSubDirectoryType(cache_entry),
                                   CACHED_FILE_LOCALLY_MODIFIED);
    } else {
      // If file was pinned before but actual file blob doesn't exist in cache:
      // - don't need to move the file.
      // - don't create symlink since it already exists.
      if (!cache_entry.is_present()) {
        DCHECK(cache_entry.is_pinned());
        return DRIVE_FILE_OK;
      }
      // File exists, move it to persistent dir.
      // Gets the current path of the file in cache.
      base::FilePath source_path = GetCacheFilePath(resource_id,
                                              md5,
                                              GetSubDirectoryType(cache_entry),
                                              CACHED_FILE_FROM_SERVER);
      dest_path = GetCacheFilePath(resource_id,
                                   md5,
                                   CACHE_TYPE_PERSISTENT,
                                   CACHED_FILE_FROM_SERVER);
      if (!MoveFile(source_path, dest_path))
        return DRIVE_FILE_ERROR_FAILED;
    }
  }

  // Create symlink in pinned dir.
  base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_PINNED,
                                           CACHED_FILE_FROM_SERVER);
  DCHECK(!dest_path.empty());
  if (!CreateSymlink(dest_path, symlink_path))
    return DRIVE_FILE_ERROR_FAILED;

  // Now that file operations have completed, update metadata.
  cache_entry.set_md5(md5);
  cache_entry.set_is_pinned(true);
  cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
  metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  return DRIVE_FILE_OK;
}

DriveFileError DriveCache::UnpinOnBlockingPool(const std::string& resource_id,
                                               const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // Unpinning a file means its entry must exist in cache.
  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry)) {
    LOG(WARNING) << "Can't unpin a file that wasn't pinned or cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_NOT_FOUND;
  }

  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_TMP;

  // If file is dirty or mounted, don't move it.
  if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
    sub_dir_type = CACHE_TYPE_PERSISTENT;
    DCHECK(cache_entry.is_persistent());
  } else {
    // If file was pinned but actual file blob still doesn't exist in cache,
    // don't need to move the file.
    if (cache_entry.is_present()) {
      // Gets the current path of the file in cache.
      base::FilePath source_path = GetCacheFilePath(resource_id,
                                              md5,
                                              GetSubDirectoryType(cache_entry),
                                              CACHED_FILE_FROM_SERVER);
      // File exists, move it to tmp dir.
      base::FilePath dest_path = GetCacheFilePath(resource_id,
                                            md5,
                                            CACHE_TYPE_TMP,
                                            CACHED_FILE_FROM_SERVER);
      if (!MoveFile(source_path, dest_path))
        return DRIVE_FILE_ERROR_FAILED;
    }
  }

  // If file was pinned, remove the symlink in pinned dir.
  if (cache_entry.is_pinned()) {
    base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_PINNED,
                                             CACHED_FILE_FROM_SERVER);
    DeleteSymlink(symlink_path);
  }

  // Now that file operations have completed, update metadata.
  if (cache_entry.is_present()) {
    cache_entry.set_md5(md5);
    cache_entry.set_is_pinned(false);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  } else {
    // Remove the existing entry if we are unpinning a non-present file.
    metadata_->RemoveCacheEntry(resource_id);
  }
  return DRIVE_FILE_OK;
}

scoped_ptr<DriveCache::GetFileResult> DriveCache::MarkAsMountedOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  scoped_ptr<GetFileResult> result(new GetFileResult);

  // Get cache entry associated with the resource_id and md5
  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry)) {
    result->first = DRIVE_FILE_ERROR_NOT_FOUND;
    return result.Pass();
  }

  if (cache_entry.is_mounted()) {
    result->first = DRIVE_FILE_ERROR_INVALID_OPERATION;
    return result.Pass();
  }

  // Get the subdir type and path for the unmounted state.
  CacheSubDirectoryType unmounted_subdir =
      cache_entry.is_pinned() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
  base::FilePath unmounted_path = GetCacheFilePath(
      resource_id, md5, unmounted_subdir, CACHED_FILE_FROM_SERVER);

  // Get the subdir type and path for the mounted state.
  CacheSubDirectoryType mounted_subdir = CACHE_TYPE_PERSISTENT;
  base::FilePath mounted_path = GetCacheFilePath(
      resource_id, md5, mounted_subdir, CACHED_FILE_MOUNTED);

  // Move cache file.
  bool success = MoveFile(unmounted_path, mounted_path);

  if (success) {
    // Now that cache operation is complete, update metadata.
    cache_entry.set_md5(md5);
    cache_entry.set_is_mounted(true);
    cache_entry.set_is_persistent(true);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
  result->first = success ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
  result->second = mounted_path;
  return result.Pass();
}

DriveFileError DriveCache::MarkAsUnmountedOnBlockingPool(
    const base::FilePath& file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(IsUnderDriveCacheDirectory(file_path));

  // Parse file path to obtain resource_id, md5 and extra_extension.
  std::string resource_id;
  std::string md5;
  std::string extra_extension;
  util::ParseCacheFilePath(file_path, &resource_id, &md5, &extra_extension);
  // The extra_extension shall be ".mounted" iff we're unmounting.
  DCHECK_EQ(util::kMountedArchiveFileExtension, extra_extension);

  // Get cache entry associated with the resource_id and md5
  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, md5, &cache_entry))
    return DRIVE_FILE_ERROR_NOT_FOUND;

  if (!cache_entry.is_mounted())
    return DRIVE_FILE_ERROR_INVALID_OPERATION;

  // Get the subdir type and path for the unmounted state.
  CacheSubDirectoryType unmounted_subdir =
      cache_entry.is_pinned() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
  base::FilePath unmounted_path = GetCacheFilePath(
      resource_id, md5, unmounted_subdir, CACHED_FILE_FROM_SERVER);

  // Get the subdir type and path for the mounted state.
  CacheSubDirectoryType mounted_subdir = CACHE_TYPE_PERSISTENT;
  base::FilePath mounted_path = GetCacheFilePath(
      resource_id, md5, mounted_subdir, CACHED_FILE_MOUNTED);

  // Move cache file.
  if (!MoveFile(mounted_path, unmounted_path))
    return DRIVE_FILE_ERROR_FAILED;

  // Now that cache operation is complete, update metadata.
  cache_entry.set_md5(md5);
  cache_entry.set_is_mounted(false);
  cache_entry.set_is_persistent(unmounted_subdir == CACHE_TYPE_PERSISTENT);
  metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  return DRIVE_FILE_OK;
}

DriveFileError DriveCache::MarkDirtyOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_NOT_FOUND;
  }

  // If a file is already dirty (i.e. MarkDirtyInCache was called before),
  // delete outgoing symlink if it exists.
  // TODO(benchan): We should only delete outgoing symlink if file is currently
  // not being uploaded.  However, for now, cache doesn't know if uploading of a
  // file is in progress.  Per zel, the upload process should be canceled before
  // MarkDirtyInCache is called again.
  if (cache_entry.is_dirty()) {
    // The file must be in persistent dir.
    DCHECK(cache_entry.is_persistent());

    // Determine symlink path in outgoing dir, so as to remove it.
    base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_OUTGOING,
                                             CACHED_FILE_FROM_SERVER);
    DeleteSymlink(symlink_path);

    return DRIVE_FILE_OK;
  }

  // Move file to persistent dir with new .local extension.

  // Get the current path of the file in cache.
  base::FilePath source_path = GetCacheFilePath(resource_id,
                                          md5,
                                          GetSubDirectoryType(cache_entry),
                                          CACHED_FILE_FROM_SERVER);
  // Determine destination path.
  const CacheSubDirectoryType sub_dir_type = CACHE_TYPE_PERSISTENT;
  base::FilePath cache_file_path = GetCacheFilePath(resource_id,
                                              md5,
                                              sub_dir_type,
                                              CACHED_FILE_LOCALLY_MODIFIED);

  bool success = MoveFile(source_path, cache_file_path);

  // If file is pinned, update symlink in pinned dir.
  if (success && cache_entry.is_pinned()) {
    base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_PINNED,
                                             CACHED_FILE_FROM_SERVER);
    success = CreateSymlink(cache_file_path, symlink_path);
  }

  if (success) {
    // Now that file operations have completed, update metadata.
    cache_entry.set_md5(md5);
    cache_entry.set_is_dirty(true);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
  return success ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
}

DriveFileError DriveCache::CommitDirtyOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.

  // Committing a file dirty means its entry and actual file blob must exist in
  // cache.
  DriveCacheEntry cache_entry;
  if (!GetCacheEntryOnBlockingPool(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't commit dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), committing it dirty is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't commit a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_INVALID_OPERATION;
  }

  // Dirty files must be in persistent dir.
  DCHECK(cache_entry.is_persistent());

  // Create symlink in outgoing dir.
  base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_OUTGOING,
                                           CACHED_FILE_FROM_SERVER);

  // Get target path of symlink i.e. current path of the file in cache.
  base::FilePath target_path = GetCacheFilePath(resource_id,
                                          md5,
                                          GetSubDirectoryType(cache_entry),
                                          CACHED_FILE_LOCALLY_MODIFIED);

  return CreateSymlink(target_path, symlink_path) ?
      DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
}

DriveFileError DriveCache::ClearDirtyOnBlockingPool(
    const std::string& resource_id,
    const std::string& md5) {
  AssertOnSequencedWorkerPool();

  // |md5| is the new .<md5> extension to rename the file to.
  // So, search for entry in cache without comparing md5.
  DriveCacheEntry cache_entry;

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  if (!GetCacheEntryOnBlockingPool(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << "res_id=" << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    return DRIVE_FILE_ERROR_INVALID_OPERATION;
  }

  // File must be dirty and hence in persistent dir.
  DCHECK(cache_entry.is_persistent());

  // Get the current path of the file in cache.
  base::FilePath source_path = GetCacheFilePath(resource_id,
                                          md5,
                                          GetSubDirectoryType(cache_entry),
                                          CACHED_FILE_LOCALLY_MODIFIED);

  // Determine destination path.
  // If file is pinned, move it to persistent dir with .md5 extension;
  // otherwise, move it to tmp dir with .md5 extension.
  const CacheSubDirectoryType sub_dir_type =
      cache_entry.is_pinned() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
  base::FilePath dest_path = GetCacheFilePath(resource_id,
                                        md5,
                                        sub_dir_type,
                                        CACHED_FILE_FROM_SERVER);

  bool success = MoveFile(source_path, dest_path);

  if (success) {
    // Delete symlink in outgoing dir.
    base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_OUTGOING,
                                             CACHED_FILE_FROM_SERVER);
    DeleteSymlink(symlink_path);
  }

  // If file is pinned, update symlink in pinned dir.
  if (success && cache_entry.is_pinned()) {
    base::FilePath symlink_path = GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_PINNED,
                                             CACHED_FILE_FROM_SERVER);

    success = CreateSymlink(dest_path, symlink_path);
  }

  if (success) {
    // Now that file operations have completed, update metadata.
    cache_entry.set_md5(md5);
    cache_entry.set_is_dirty(false);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }

  return success ? DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
}

DriveFileError DriveCache::RemoveOnBlockingPool(
    const std::string& resource_id) {
  AssertOnSequencedWorkerPool();

  // MD5 is not passed into RemoveCacheEntry because we would delete all
  // cache files corresponding to <resource_id> regardless of the md5.
  // So, search for entry in cache without taking md5 into account.
  DriveCacheEntry cache_entry;

  // If entry doesn't exist or is dirty or mounted in cache, nothing to do.
  const bool entry_found =
      GetCacheEntryOnBlockingPool(resource_id, std::string(), &cache_entry);
  if (!entry_found || cache_entry.is_dirty() || cache_entry.is_mounted()) {
    DVLOG(1) << "Entry is "
             << (entry_found ?
                 (cache_entry.is_dirty() ? "dirty" : "mounted") :
                 "non-existent")
             << " in cache, not removing";
    return DRIVE_FILE_OK;
  }

  // Determine paths to delete all cache versions of |resource_id| in
  // persistent, tmp and pinned directories.
  std::vector<base::FilePath> paths_to_delete;

  // For files in persistent and tmp dirs, delete files that match
  // "<resource_id>.*".
  paths_to_delete.push_back(GetCacheFilePath(resource_id,
                                             util::kWildCard,
                                             CACHE_TYPE_PERSISTENT,
                                             CACHED_FILE_FROM_SERVER));
  paths_to_delete.push_back(GetCacheFilePath(resource_id,
                                             util::kWildCard,
                                             CACHE_TYPE_TMP,
                                             CACHED_FILE_FROM_SERVER));

  // For pinned files, filename is "<resource_id>" with no extension, so delete
  // "<resource_id>".
  paths_to_delete.push_back(GetCacheFilePath(resource_id,
                                             std::string(),
                                             CACHE_TYPE_PINNED,
                                             CACHED_FILE_FROM_SERVER));

  // Don't delete locally modified (i.e. dirty and possibly outgoing) files.
  // Since we're not deleting outgoing symlinks, we don't need to append
  // outgoing path to |paths_to_delete|.
  base::FilePath path_to_keep = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_PERSISTENT,
                                           CACHED_FILE_LOCALLY_MODIFIED);

  for (size_t i = 0; i < paths_to_delete.size(); ++i) {
    DeleteFilesSelectively(paths_to_delete[i], path_to_keep);
  }

  // Now that all file operations have completed, remove from metadata.
  metadata_->RemoveCacheEntry(resource_id);

  return DRIVE_FILE_OK;
}

bool DriveCache::ClearAllOnBlockingPool() {
  AssertOnSequencedWorkerPool();

  if (!file_util::Delete(cache_root_path_, true)) {
    LOG(WARNING) << "Failed to delete the cache directory";
    return false;
  }

  if (!InitializeOnBlockingPool()) {
    LOG(WARNING) << "Failed to initialize the cache";
    return false;
  }
  return true;
}

void DriveCache::OnPinned(const std::string& resource_id,
                          const std::string& md5,
                          const FileOperationCallback& callback,
                          DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error);

  if (error == DRIVE_FILE_OK)
    FOR_EACH_OBSERVER(DriveCacheObserver,
                      observers_,
                      OnCachePinned(resource_id, md5));
}

void DriveCache::OnUnpinned(const std::string& resource_id,
                            const std::string& md5,
                            const FileOperationCallback& callback,
                            DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error);

  if (error == DRIVE_FILE_OK)
    FOR_EACH_OBSERVER(DriveCacheObserver,
                      observers_,
                      OnCacheUnpinned(resource_id, md5));

  // Now the file is moved from "persistent" to "tmp" directory.
  // It's a chance to free up space if needed.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(
              &DriveCache::FreeDiskSpaceOnBlockingPoolIfNeededFor),
          base::Unretained(this), 0));
}

void DriveCache::OnCommitDirty(const std::string& resource_id,
                               const FileOperationCallback& callback,
                               DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error);

  if (error == DRIVE_FILE_OK)
    FOR_EACH_OBSERVER(DriveCacheObserver,
                      observers_,
                      OnCacheCommitted(resource_id));
}

bool DriveCache::HasEnoughSpaceFor(int64 num_bytes,
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

// static
base::FilePath DriveCache::GetCacheRootPath(Profile* profile) {
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  base::FilePath cache_root_path =
      cache_base_path.Append(chrome::kDriveCacheDirname);
  return cache_root_path.Append(kDriveCacheVersionDir);
}

// static
std::vector<base::FilePath> DriveCache::GetCachePaths(
    const base::FilePath& cache_root_path) {
  std::vector<base::FilePath> cache_paths;
  // The order should match DriveCache::CacheSubDirectoryType enum.
  cache_paths.push_back(cache_root_path.Append(kDriveCacheMetaDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCachePinnedDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCacheOutgoingDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCachePersistentDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCacheTmpDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCacheTmpDownloadsDir));
  cache_paths.push_back(cache_root_path.Append(kDriveCacheTmpDocumentsDir));
  return cache_paths;
}

// static
bool DriveCache::CreateCacheDirectories(
    const std::vector<base::FilePath>& paths_to_create) {
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

// static
DriveCache::CacheSubDirectoryType DriveCache::GetSubDirectoryType(
    const DriveCacheEntry& cache_entry) {
  return cache_entry.is_persistent() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
}

}  // namespace drive
