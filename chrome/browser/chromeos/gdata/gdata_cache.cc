// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache.h"

#include <vector>

#include "base/chromeos/chromeos_version.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_cache_metadata.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {
namespace {

const FilePath::CharType kGDataCacheVersionDir[] = FILE_PATH_LITERAL("v1");
const FilePath::CharType kGDataCacheMetaDir[] = FILE_PATH_LITERAL("meta");
const FilePath::CharType kGDataCachePinnedDir[] = FILE_PATH_LITERAL("pinned");
const FilePath::CharType kGDataCacheOutgoingDir[] =
    FILE_PATH_LITERAL("outgoing");
const FilePath::CharType kGDataCachePersistentDir[] =
    FILE_PATH_LITERAL("persistent");
const FilePath::CharType kGDataCacheTmpDir[] = FILE_PATH_LITERAL("tmp");
const FilePath::CharType kGDataCacheTmpDownloadsDir[] =
    FILE_PATH_LITERAL("tmp/downloads");
const FilePath::CharType kGDataCacheTmpDocumentsDir[] =
    FILE_PATH_LITERAL("tmp/documents");

// Returns the home directory path, or an empty string if the home directory
// is not found.
// Copied from webkit/chromeos/cros_mount_point_provider.h.
// TODO(satorux): Share the code.
std::string GetHomeDirectory() {
  if (base::chromeos::IsRunningOnChromeOS())
    return "/home/chronos/user";

  const char* home = getenv("HOME");
  if (home)
    return home;
  return "";
}

// Used to tweak GetAmountOfFreeDiskSpace() behavior for testing.
FreeDiskSpaceGetterInterface* global_free_disk_getter_for_testing = NULL;

// Gets the amount of free disk space. Use
// |global_free_disk_getter_for_testing| if set.
int64 GetAmountOfFreeDiskSpace() {
  if (global_free_disk_getter_for_testing)
    return global_free_disk_getter_for_testing->AmountOfFreeDiskSpace();

  return base::SysInfo::AmountOfFreeDiskSpace(
      FilePath::FromUTF8Unsafe(GetHomeDirectory()));
}

// Returns true if we have sufficient space to store the given number of
// bytes, while keeping kMinFreeSpace bytes on the disk.
bool HasEnoughSpaceFor(int64 num_bytes) {
  int64 free_space = GetAmountOfFreeDiskSpace();
  // Substract this as if this portion does not exist.
  free_space -= kMinFreeSpace;
  return (free_space >= num_bytes);
}

// Create cache directory paths and set permissions.
void InitCachePaths(const std::vector<FilePath>& cache_paths) {
  if (cache_paths.size() < GDataCache::NUM_CACHE_TYPES) {
    NOTREACHED();
    LOG(ERROR) << "Size of cache_paths is invalid.";
    return;
  }

  if (!GDataCache::CreateCacheDirectories(cache_paths))
    return;

  // Change permissions of cache persistent directory to u+rwx,og+x (711) in
  // order to allow archive files in that directory to be mounted by cros-disks.
  file_util::SetPosixFilePermissions(
      cache_paths[GDataCache::CACHE_TYPE_PERSISTENT],
      file_util::FILE_PERMISSION_USER_MASK |
      file_util::FILE_PERMISSION_EXECUTE_BY_GROUP |
      file_util::FILE_PERMISSION_EXECUTE_BY_OTHERS);
}

// Remove all files under the given directory, non-recursively.
// Do not remove recursively as we don't want to touch <gache>/tmp/downloads,
// which is used for user initiated downloads like "Save As"
void RemoveAllFiles(const FilePath& directory) {
  using file_util::FileEnumerator;

  FileEnumerator enumerator(directory, false /* recursive */,
                            FileEnumerator::FILES);
  for (FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    DVLOG(1) << "Removing " << file_path.value();
    if (!file_util::Delete(file_path, false /* recursive */))
      LOG(WARNING) << "Failed to delete " << file_path.value();
  }
}

// Modifies cache state of file on blocking pool, which involves:
// - moving or copying file (per |file_operation_type|) from |source_path| to
//  |dest_path| if they're different
// - deleting symlink if |symlink_path| is not empty
// - creating symlink if |symlink_path| is not empty and |create_symlink| is
//   true.
GDataFileError ModifyCacheState(
    const FilePath& source_path,
    const FilePath& dest_path,
    GDataCache::FileOperationType file_operation_type,
    const FilePath& symlink_path,
    bool create_symlink) {
  // Move or copy |source_path| to |dest_path| if they are different.
  if (source_path != dest_path) {
    bool success = false;
    if (file_operation_type == GDataCache::FILE_OPERATION_MOVE)
      success = file_util::Move(source_path, dest_path);
    else if (file_operation_type == GDataCache::FILE_OPERATION_COPY)
      success = file_util::CopyFile(source_path, dest_path);
    if (!success) {
      LOG(ERROR) << "Failed to "
                 << (file_operation_type == GDataCache::FILE_OPERATION_MOVE ?
                     "move " : "copy ")
                 << source_path.value()
                 << " to " << dest_path.value();
      return GDATA_FILE_ERROR_FAILED;
    } else {
      DVLOG(1) << (file_operation_type == GDataCache::FILE_OPERATION_MOVE ?
                   "Moved " : "Copied ")
               << source_path.value()
               << " to " << dest_path.value();
    }
  } else {
    DVLOG(1) << "No need to move file: source = destination";
  }

  if (symlink_path.empty())
    return GDATA_FILE_OK;

  // Remove symlink regardless of |create_symlink| because creating a link will
  // not overwrite an existing one.
  // We try to save one file operation by not checking if link exists before
  // deleting it, so unlink may return error if link doesn't exist, but it
  // doesn't really matter to us.
  file_util::Delete(symlink_path, false);

  if (!create_symlink)
    return GDATA_FILE_OK;

  // Create new symlink to |dest_path|.
  if (!file_util::CreateSymbolicLink(dest_path, symlink_path)) {
    LOG(ERROR) << "Failed to create a symlink from " << symlink_path.value()
               << " to " << dest_path.value();
    return GDATA_FILE_ERROR_FAILED;
  }

  return GDATA_FILE_OK;
}

// Deletes all files that match |path_to_delete_pattern| except for
// |path_to_keep| on blocking pool.
// If |path_to_keep| is empty, all files in |path_to_delete_pattern| are
// deleted.
void DeleteFilesSelectively(const FilePath& path_to_delete_pattern,
                            const FilePath& path_to_keep) {
  // Enumerate all files in directory of |path_to_delete_pattern| that match
  // base name of |path_to_delete_pattern|.
  // If a file is not |path_to_keep|, delete it.
  bool success = true;
  file_util::FileEnumerator enumerator(
      path_to_delete_pattern.DirName(),
      false,  // not recursive
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::SHOW_SYM_LINKS),
      path_to_delete_pattern.BaseName().value());
  for (FilePath current = enumerator.Next(); !current.empty();
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

// Appends |resource_id| ID to |to_fetch| if the file is pinned but not
// fetched (not present locally), or to |to_upload| if the file is dirty
// but not uploaded.
void CollectBacklog(std::vector<std::string>* to_fetch,
                    std::vector<std::string>* to_upload,
                    const std::string& resource_id,
                    const GDataCacheEntry& cache_entry) {
  DCHECK(to_fetch);
  DCHECK(to_upload);

  if (cache_entry.is_pinned() && !cache_entry.is_present())
    to_fetch->push_back(resource_id);

  if (cache_entry.is_dirty())
    to_upload->push_back(resource_id);
}

// Appends |resource_id| ID to |resource_ids| if the file is pinned and
// present (cached locally).
void CollectExistingPinnedFile(std::vector<std::string>* resource_ids,
                               const std::string& resource_id,
                               const GDataCacheEntry& cache_entry) {
  DCHECK(resource_ids);

  if (cache_entry.is_pinned() && cache_entry.is_present())
    resource_ids->push_back(resource_id);
}

// Runs callback with pointers dereferenced.
// Used to implement SetMountedStateOnUIThread.
void RunSetMountedStateCallback(const SetMountedStateCallback& callback,
                                GDataFileError* error,
                                FilePath* cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  DCHECK(cache_file_path);

  if (!callback.is_null())
    callback.Run(*error, *cache_file_path);
}

// Runs callback with pointers dereferenced.
// Used to implement *OnUIThread methods.
void RunCacheOperationCallback(const CacheOperationCallback& callback,
                               GDataFileError* error,
                               const std::string& resource_id,
                               const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);
}

// Runs callback with pointers dereferenced.
// Used to implement *OnUIThread methods.
void RunGetFileFromCacheCallback(const GetFileFromCacheCallback& callback,
                                 GDataFileError* error,
                                 const std::string& resource_id,
                                 const std::string& md5,
                                 FilePath* cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);
  DCHECK(cache_file_path);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5, *cache_file_path);
}

// Runs callback with pointers dereferenced.
// Used to implement GetResourceIdsOfBacklogOnUIThread().
void RunGetResourceIdsOfBacklogCallback(
    const GetResourceIdsOfBacklogCallback& callback,
    std::vector<std::string>* to_fetch,
    std::vector<std::string>* to_upload) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(to_fetch);
  DCHECK(to_upload);

  if (!callback.is_null())
    callback.Run(*to_fetch, *to_upload);
}

// Runs callback with pointers dereferenced.
// Used to implement GetResourceIdsOfExistingPinnedFilesOnUIThread().
void RunGetResourceIdsCallback(
    const GetResourceIdsCallback& callback,
    std::vector<std::string>* resource_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(resource_ids);

  if (!callback.is_null())
    callback.Run(*resource_ids);
}

// Runs callback with pointers dereferenced.
// Used to implement GetCacheEntryOnUIThread().
void RunGetCacheEntryCallback(
    const GetCacheEntryCallback& callback,
    bool* success,
    GDataCacheEntry* cache_entry) {
  DCHECK(success);
  DCHECK(cache_entry);

  if (!callback.is_null())
    callback.Run(*success, *cache_entry);
}

}  // namespace

GDataCache::GDataCache(const FilePath& cache_root_path,
                       base::SequencedTaskRunner* blocking_task_runner)
    : cache_root_path_(cache_root_path),
      cache_paths_(GetCachePaths(cache_root_path_)),
      blocking_task_runner_(blocking_task_runner),
      ui_weak_ptr_factory_(this),
      ui_weak_ptr_(ui_weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataCache::~GDataCache() {
  AssertOnSequencedWorkerPool();
}

FilePath GDataCache::GetCacheDirectoryPath(
    CacheSubDirectoryType sub_dir_type) const {
  DCHECK_LE(0, sub_dir_type);
  DCHECK_GT(NUM_CACHE_TYPES, sub_dir_type);
  return cache_paths_[sub_dir_type];
}

FilePath GDataCache::GetCacheFilePath(const std::string& resource_id,
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
    base_name += FilePath::kExtensionSeparator;
    base_name += util::kLocallyModifiedFileExtension;
  } else if (!md5.empty()) {
    base_name += FilePath::kExtensionSeparator;
    base_name += util::EscapeCacheFileName(md5);
  }
  // For mounted archives the filename is formatted as resource_id.md5.mounted,
  // i.e. resource_id.md5 is the base name and ".mounted" is the extension
  if (file_origin == CACHED_FILE_MOUNTED) {
    DCHECK(sub_dir_type == CACHE_TYPE_PERSISTENT);
    base_name += FilePath::kExtensionSeparator;
    base_name += util::kMountedArchiveFileExtension;
  }
  return GetCacheDirectoryPath(sub_dir_type).Append(base_name);
}

void GDataCache::AssertOnSequencedWorkerPool() {
  DCHECK(!blocking_task_runner_ ||
         blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool GDataCache::IsUnderGDataCacheDirectory(const FilePath& path) const {
  return cache_root_path_ == path || cache_root_path_.IsParent(path);
}

void GDataCache::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void GDataCache::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void GDataCache::GetCacheEntryOnUIThread(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool* success = new bool(false);
  GDataCacheEntry* cache_entry = new GDataCacheEntry;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::GetCacheEntryHelper,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 success,
                 cache_entry),
      base::Bind(&RunGetCacheEntryCallback,
                 callback,
                 base::Owned(success),
                 base::Owned(cache_entry)));
}

void GDataCache::GetResourceIdsOfBacklogOnUIThread(
    const GetResourceIdsOfBacklogCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string>* to_fetch = new std::vector<std::string>;
  std::vector<std::string>* to_upload = new std::vector<std::string>;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::GetResourceIdsOfBacklog,
                 base::Unretained(this),
                 to_fetch,
                 to_upload),
      base::Bind(&RunGetResourceIdsOfBacklogCallback,
                 callback,
                 base::Owned(to_fetch),
                 base::Owned(to_upload)));
}

void GDataCache::GetResourceIdsOfExistingPinnedFilesOnUIThread(
    const GetResourceIdsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string>* resource_ids = new std::vector<std::string>;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::GetResourceIdsOfExistingPinnedFiles,
                 base::Unretained(this),
                 resource_ids),
      base::Bind(&RunGetResourceIdsCallback,
                 callback,
                 base::Owned(resource_ids)));
}

void GDataCache::FreeDiskSpaceIfNeededFor(int64 num_bytes,
                                          bool* has_enough_space) {
  AssertOnSequencedWorkerPool();

  // Do nothing and return if we have enough space.
  *has_enough_space = HasEnoughSpaceFor(num_bytes);
  if (*has_enough_space)
    return;

  // Otherwise, try to free up the disk space.
  DVLOG(1) << "Freeing up disk space for " << num_bytes;
  // First remove temporary files from the cache map.
  metadata_->RemoveTemporaryFiles();
  // Then remove all files under "tmp" directory.
  RemoveAllFiles(GetCacheDirectoryPath(GDataCache::CACHE_TYPE_TMP));

  // Check the disk space again.
  *has_enough_space = HasEnoughSpaceFor(num_bytes);
}

void GDataCache::GetFileOnUIThread(const std::string& resource_id,
                                   const std::string& md5,
                                   const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::GetFile,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 error,
                 cache_file_path),
      base::Bind(&RunGetFileFromCacheCallback,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5,
                 base::Owned(cache_file_path)));
}

void GDataCache::StoreOnUIThread(const std::string& resource_id,
                                 const std::string& md5,
                                 const FilePath& source_path,
                                 FileOperationType file_operation_type,
                                 const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::Store,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 source_path,
                 file_operation_type,
                 error),
      base::Bind(&RunCacheOperationCallback,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5));
}

void GDataCache::PinOnUIThread(const std::string& resource_id,
                               const std::string& md5,
                               const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::Pin,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 GDataCache::FILE_OPERATION_MOVE,
                 error),
      base::Bind(&GDataCache::OnPinned,
                 ui_weak_ptr_,
                 base::Owned(error),
                 resource_id,
                 md5,
                 callback));
}

void GDataCache::UnpinOnUIThread(const std::string& resource_id,
                                 const std::string& md5,
                                 const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::Unpin,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 GDataCache::FILE_OPERATION_MOVE,
                 error),
      base::Bind(&GDataCache::OnUnpinned,
                 ui_weak_ptr_,
                 base::Owned(error),
                 resource_id,
                 md5,
                 callback));
}

void GDataCache::SetMountedStateOnUIThread(
    const FilePath& file_path,
    bool to_mount,
    const SetMountedStateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::SetMountedState,
                 base::Unretained(this),
                 file_path,
                 to_mount,
                 error,
                 cache_file_path),
      base::Bind(&RunSetMountedStateCallback,
                 callback,
                 base::Owned(error),
                 base::Owned(cache_file_path)));
}

void GDataCache::MarkDirtyOnUIThread(const std::string& resource_id,
                                     const std::string& md5,
                                     const GetFileFromCacheCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  FilePath* cache_file_path = new FilePath;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::MarkDirty,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 GDataCache::FILE_OPERATION_MOVE,
                 error,
                 cache_file_path),
      base::Bind(&RunGetFileFromCacheCallback,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5,
                 base::Owned(cache_file_path)));
}

void GDataCache::CommitDirtyOnUIThread(const std::string& resource_id,
                                       const std::string& md5,
                                       const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error = new GDataFileError(GDATA_FILE_OK);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::CommitDirty,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 GDataCache::FILE_OPERATION_MOVE,
                 error),
      base::Bind(&GDataCache::OnCommitDirty,
                 ui_weak_ptr_,
                 base::Owned(error),
                 resource_id,
                 md5,
                 callback));
}

void GDataCache::ClearDirtyOnUIThread(const std::string& resource_id,
                                      const std::string& md5,
                                      const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::ClearDirty,
                 base::Unretained(this),
                 resource_id,
                 md5,
                 GDataCache::FILE_OPERATION_MOVE,
                 error),
      base::Bind(&RunCacheOperationCallback,
                 callback,
                 base::Owned(error),
                 resource_id,
                 md5));
}

void GDataCache::RemoveOnUIThread(const std::string& resource_id,
                                  const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError* error =
      new GDataFileError(GDATA_FILE_OK);

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GDataCache::Remove,
                 base::Unretained(this),
                 resource_id,
                 error),
      base::Bind(&RunCacheOperationCallback,
                 callback,
                 base::Owned(error),
                 resource_id,
                 ""  /* md5 */));
}

void GDataCache::RequestInitializeOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GDataCache::Initialize, base::Unretained(this)));
}

void GDataCache::ForceRescanOnUIThreadForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GDataCache::ForceRescanForTesting, base::Unretained(this)));
}

bool GDataCache::GetCacheEntry(const std::string& resource_id,
                               const std::string& md5,
                               GDataCacheEntry* entry) {
  DCHECK(entry);
  AssertOnSequencedWorkerPool();
  return metadata_->GetCacheEntry(resource_id, md5, entry);
}

// static
GDataCache* GDataCache::CreateGDataCacheOnUIThread(
    const FilePath& cache_root_path,
    base::SequencedTaskRunner* blocking_task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new GDataCache(cache_root_path, blocking_task_runner);
}

void GDataCache::DestroyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Invalidate the weak pointer.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy myself on the blocking pool.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GDataCache::Destroy,
                 base::Unretained(this)));
}

void GDataCache::Initialize() {
  AssertOnSequencedWorkerPool();

  InitCachePaths(cache_paths_);
  metadata_ = GDataCacheMetadata::CreateGDataCacheMetadata(
      blocking_task_runner_).Pass();
  metadata_->Initialize(cache_paths_);
}

void GDataCache::Destroy() {
  AssertOnSequencedWorkerPool();
  delete this;
}

void GDataCache::ForceRescanForTesting() {
  AssertOnSequencedWorkerPool();
  metadata_->ForceRescanForTesting(cache_paths_);
}

void GDataCache::GetResourceIdsOfBacklog(
    std::vector<std::string>* to_fetch,
    std::vector<std::string>* to_upload) {
  AssertOnSequencedWorkerPool();
  DCHECK(to_fetch);
  DCHECK(to_upload);

  metadata_->Iterate(base::Bind(&CollectBacklog, to_fetch, to_upload));
}

void GDataCache::GetResourceIdsOfExistingPinnedFiles(
    std::vector<std::string>* resource_ids) {
  AssertOnSequencedWorkerPool();
  DCHECK(resource_ids);

  metadata_->Iterate(base::Bind(&CollectExistingPinnedFile, resource_ids));
}

void GDataCache::GetFile(const std::string& resource_id,
                         const std::string& md5,
                         GDataFileError* error,
                         FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);
  DCHECK(cache_file_path);

  GDataCacheEntry cache_entry;
  if (GetCacheEntry(resource_id, md5, &cache_entry) &&
      cache_entry.is_present()) {
    CachedFileOrigin file_origin;
    if (cache_entry.is_mounted()) {
      file_origin = CACHED_FILE_MOUNTED;
    } else if (cache_entry.is_dirty()) {
      file_origin = CACHED_FILE_LOCALLY_MODIFIED;
    } else {
      file_origin = CACHED_FILE_FROM_SERVER;
    }
    *cache_file_path = GetCacheFilePath(
        resource_id,
        md5,
        GetSubDirectoryType(cache_entry),
        file_origin);
    *error = GDATA_FILE_OK;
  } else {
    *error = GDATA_FILE_ERROR_NOT_FOUND;
  }
}

void GDataCache::Store(const std::string& resource_id,
                       const std::string& md5,
                       const FilePath& source_path,
                       FileOperationType file_operation_type,
                       GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  FilePath dest_path;
  FilePath symlink_path;
  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_TMP;

  // If file was previously pinned, store it in persistent dir and create
  // symlink in pinned dir.
  GDataCacheEntry cache_entry;
  if (GetCacheEntry(resource_id, md5, &cache_entry)) {  // File exists in cache.
    // If file is dirty or mounted, return error.
    if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
      LOG(WARNING) << "Can't store a file to replace a "
                   << (cache_entry.is_dirty() ? "dirty" : "mounted")
                   << " file: res_id=" << resource_id
                   << ", md5=" << md5;
      *error = GDATA_FILE_ERROR_IN_USE;
      return;
    }

    // If file is pinned, determines destination path.
    if (cache_entry.is_pinned()) {
      sub_dir_type = CACHE_TYPE_PERSISTENT;
      dest_path = GetCacheFilePath(resource_id, md5, sub_dir_type,
                                   CACHED_FILE_FROM_SERVER);
      symlink_path = GetCacheFilePath(
          resource_id, std::string(), CACHE_TYPE_PINNED,
          CACHED_FILE_FROM_SERVER);
    }
  }

  // File wasn't pinned or doesn't exist in cache, store in tmp dir.
  if (dest_path.empty()) {
    DCHECK_EQ(CACHE_TYPE_TMP, sub_dir_type);
    dest_path = GetCacheFilePath(resource_id, md5, sub_dir_type,
                                 CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      dest_path,
      file_operation_type,
      symlink_path,
      !symlink_path.empty());  // create symlink

  // Determine search pattern for stale filenames corrresponding to resource_id,
  // either "<resource_id>*" or "<resource_id>.*".
  FilePath stale_filenames_pattern;
  if (md5.empty()) {
    // No md5 means no extension, append '*' after base name, i.e.
    // "<resource_id>*".
    // Cannot call |dest_path|.ReplaceExtension when there's no md5 extension:
    // if base name of |dest_path| (i.e. escaped resource_id) contains the
    // extension separator '.', ReplaceExtension will remove it and everything
    // after it.  The result will be nothing like the escaped resource_id.
    stale_filenames_pattern = FilePath(dest_path.value() + util::kWildCard);
  } else {
    // Replace md5 extension with '*' i.e. "<resource_id>.*".
    // Note that ReplaceExtension automatically prefixes the extension with the
    // extension separator '.'.
    stale_filenames_pattern = dest_path.ReplaceExtension(util::kWildCard);
  }

  // Delete files that match |stale_filenames_pattern| except for |dest_path|.
  DeleteFilesSelectively(stale_filenames_pattern, dest_path);

  if (*error == GDATA_FILE_OK) {
    // Now that file operations have completed, update cache map.
    cache_entry.set_md5(md5);
    cache_entry.set_is_present(true);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
}

void GDataCache::Pin(const std::string& resource_id,
                     const std::string& md5,
                     FileOperationType file_operation_type,
                     GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  FilePath source_path;
  FilePath dest_path;
  FilePath symlink_path;
  bool create_symlink = true;
  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_PERSISTENT;

  GDataCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, md5, &cache_entry)) {
    // Entry does not exist in cache.
    // Set both |dest_path| and |source_path| to /dev/null, so that:
    // 1) ModifyCacheState won't move files when |source_path| and |dest_path|
    //    are the same.
    // 2) symlinks to /dev/null will be picked up by GDataSyncClient to download
    //    pinned files that don't exist in cache.
    dest_path = FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull);
    source_path = dest_path;

    // Set sub_dir_type to TMP. The file will be first downloaded in 'tmp',
    // then moved to 'persistent'.
    sub_dir_type = CACHE_TYPE_TMP;
  } else {  // File exists in cache, determines destination path.
    // Determine source and destination paths.

    // If file is dirty or mounted, don't move it, so determine |dest_path| and
    // set |source_path| the same, because ModifyCacheState only moves files if
    // source and destination are different.
    if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
      DCHECK(cache_entry.is_persistent());
      dest_path = GetCacheFilePath(resource_id,
                                   md5,
                                   GetSubDirectoryType(cache_entry),
                                   CACHED_FILE_LOCALLY_MODIFIED);
      source_path = dest_path;
    } else {
      // Gets the current path of the file in cache.
      source_path = GetCacheFilePath(resource_id,
                                     md5,
                                     GetSubDirectoryType(cache_entry),
                                     CACHED_FILE_FROM_SERVER);

      // If file was pinned before but actual file blob doesn't exist in cache:
      // - don't need to move the file, so set |dest_path| to |source_path|,
      //   because ModifyCacheState only moves files if source and destination
      //   are different
      // - don't create symlink since it already exists.
      if (!cache_entry.is_present()) {
        dest_path = source_path;
        create_symlink = false;
      } else {  // File exists, move it to persistent dir.
        dest_path = GetCacheFilePath(resource_id,
                                     md5,
                                     CACHE_TYPE_PERSISTENT,
                                     CACHED_FILE_FROM_SERVER);
      }
    }
  }

  // Create symlink in pinned dir.
  if (create_symlink) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(source_path,
                            dest_path,
                            file_operation_type,
                            symlink_path,
                            create_symlink);

  if (*error == GDATA_FILE_OK) {
    // Now that file operations have completed, update cache map.
    cache_entry.set_md5(md5);
    cache_entry.set_is_pinned(true);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
}

void GDataCache::Unpin(const std::string& resource_id,
                       const std::string& md5,
                       FileOperationType file_operation_type,
                       GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  // Unpinning a file means its entry must exist in cache.
  GDataCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, md5, &cache_entry)) {
    LOG(WARNING) << "Can't unpin a file that wasn't pinned or cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }

  // Entry exists in cache, determines source and destination paths.

  FilePath source_path;
  FilePath dest_path;
  CacheSubDirectoryType sub_dir_type = CACHE_TYPE_TMP;

  // If file is dirty or mounted, don't move it, so determine |dest_path| and
  // set |source_path| the same, because ModifyCacheState moves files if source
  // and destination are different.
  if (cache_entry.is_dirty() || cache_entry.is_mounted()) {
    sub_dir_type = CACHE_TYPE_PERSISTENT;
    DCHECK(cache_entry.is_persistent());
    dest_path = GetCacheFilePath(resource_id,
                                 md5,
                                 GetSubDirectoryType(cache_entry),
                                 CACHED_FILE_LOCALLY_MODIFIED);
    source_path = dest_path;
  } else {
    // Gets the current path of the file in cache.
    source_path = GetCacheFilePath(resource_id,
                                   md5,
                                   GetSubDirectoryType(cache_entry),
                                   CACHED_FILE_FROM_SERVER);

    // If file was pinned but actual file blob still doesn't exist in cache,
    // don't need to move the file, so set |dest_path| to |source_path|, because
    // ModifyCacheState only moves files if source and destination are
    // different.
    if (!cache_entry.is_present()) {
      dest_path = source_path;
    } else {  // File exists, move it to tmp dir.
      dest_path = GetCacheFilePath(resource_id, md5,
                                   CACHE_TYPE_TMP,
                                   CACHED_FILE_FROM_SERVER);
    }
  }

  // If file was pinned, get absolute path of symlink in pinned dir so as to
  // remove it.
  FilePath symlink_path;
  if (cache_entry.is_pinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      dest_path,
      file_operation_type,
      symlink_path,  // This will be deleted if it exists.
      false /* don't create symlink*/);

  if (*error == GDATA_FILE_OK) {
    // Now that file operations have completed, update cache map.
    if (cache_entry.is_present()) {
      cache_entry.set_md5(md5);
      cache_entry.set_is_pinned(false);
      cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
      metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
    } else {
      // Remove the existing entry if we are unpinning a non-present file.
      metadata_->RemoveCacheEntry(resource_id);
    }
  }
}

void GDataCache::SetMountedState(const FilePath& file_path,
                                 bool to_mount,
                                 GDataFileError *error,
                                 FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);
  DCHECK(cache_file_path);

  // Parse file path to obtain resource_id, md5 and extra_extension.
  std::string resource_id;
  std::string md5;
  std::string extra_extension;
  util::ParseCacheFilePath(file_path, &resource_id, &md5, &extra_extension);
  // The extra_extension shall be ".mounted" iff we're unmounting.
  DCHECK(!to_mount == (extra_extension == util::kMountedArchiveFileExtension));

  // Get cache entry associated with the resource_id and md5
  GDataCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, md5, &cache_entry)) {
    *error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }
  if (to_mount == cache_entry.is_mounted()) {
    *error = GDATA_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // Get the subdir type and path for the unmounted state.
  CacheSubDirectoryType unmounted_subdir =
      cache_entry.is_pinned() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
  FilePath unmounted_path = GetCacheFilePath(
      resource_id, md5, unmounted_subdir, CACHED_FILE_FROM_SERVER);

  // Get the subdir type and path for the mounted state.
  CacheSubDirectoryType mounted_subdir = CACHE_TYPE_PERSISTENT;
  FilePath mounted_path = GetCacheFilePath(
      resource_id, md5, mounted_subdir, CACHED_FILE_MOUNTED);

  // Determine the source and destination paths for moving the cache blob.
  FilePath source_path;
  CacheSubDirectoryType dest_subdir;
  if (to_mount) {
    source_path = unmounted_path;
    *cache_file_path = mounted_path;
    dest_subdir = mounted_subdir;
    cache_entry.set_is_mounted(true);
  } else {
    source_path = mounted_path;
    *cache_file_path = unmounted_path;
    dest_subdir = unmounted_subdir;
    cache_entry.set_is_mounted(false);
  }

  // Move cache blob from source path to destination path.
  *error = ModifyCacheState(source_path, *cache_file_path,
                            FILE_OPERATION_MOVE, FilePath(), false);
  if (*error == GDATA_FILE_OK) {
    // Now that cache operation is complete, update cache map
    cache_entry.set_md5(md5);
    cache_entry.set_is_persistent(dest_subdir == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
}

void GDataCache::MarkDirty(const std::string& resource_id,
                           const std::string& md5,
                           FileOperationType file_operation_type,
                           GDataFileError* error,
                           FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);
  DCHECK(cache_file_path);

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  GDataCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
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
    FilePath symlink_path = GetCacheFilePath(
        resource_id,
        std::string(),
        CACHE_TYPE_OUTGOING,
        CACHED_FILE_FROM_SERVER);

    // We're not moving files here, so simply use empty FilePath for both
    // |source_path| and |dest_path| because ModifyCacheState only move files
    // if source and destination are different.
    *error = ModifyCacheState(
        FilePath(),  // non-applicable source path
        FilePath(),  // non-applicable dest path
        file_operation_type,
        symlink_path,
        false /* don't create symlink */);

    // Determine current path of dirty file.
    if (*error == GDATA_FILE_OK) {
      *cache_file_path = GetCacheFilePath(
          resource_id,
          md5,
          CACHE_TYPE_PERSISTENT,
          CACHED_FILE_LOCALLY_MODIFIED);
    }
    return;
  }

  // Move file to persistent dir with new .local extension.

  // Get the current path of the file in cache.
  FilePath source_path = GetCacheFilePath(
      resource_id,
      md5,
      GetSubDirectoryType(cache_entry),
      CACHED_FILE_FROM_SERVER);

  // Determine destination path.
  const CacheSubDirectoryType sub_dir_type = CACHE_TYPE_PERSISTENT;
  *cache_file_path = GetCacheFilePath(resource_id,
                                      md5,
                                      sub_dir_type,
                                      CACHED_FILE_LOCALLY_MODIFIED);

  // If file is pinned, update symlink in pinned dir.
  FilePath symlink_path;
  if (cache_entry.is_pinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);
  }

  *error = ModifyCacheState(
      source_path,
      *cache_file_path,
      file_operation_type,
      symlink_path,
      !symlink_path.empty() /* create symlink */);

  if (*error == GDATA_FILE_OK) {
    // Now that file operations have completed, update cache map.
    cache_entry.set_md5(md5);
    cache_entry.set_is_dirty(true);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
}

void GDataCache::CommitDirty(const std::string& resource_id,
                             const std::string& md5,
                             FileOperationType file_operation_type,
                             GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  // If file has already been marked dirty in previous instance of chrome, we
  // would have lost the md5 info during cache initialization, because the file
  // would have been renamed to .local extension.
  // So, search for entry in cache without comparing md5.

  // Committing a file dirty means its entry and actual file blob must exist in
  // cache.
  GDataCacheEntry cache_entry;
  if (!GetCacheEntry(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't commit dirty a file that wasn't cached: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), committing it dirty is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't commit a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // Dirty files must be in persistent dir.
  DCHECK(cache_entry.is_persistent());

  // Create symlink in outgoing dir.
  FilePath symlink_path = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_OUTGOING,
                                           CACHED_FILE_FROM_SERVER);

  // Get target path of symlink i.e. current path of the file in cache.
  FilePath target_path = GetCacheFilePath(resource_id,
                                          md5,
                                          GetSubDirectoryType(cache_entry),
                                          CACHED_FILE_LOCALLY_MODIFIED);

  // Since there's no need to move files, use |target_path| for both
  // |source_path| and |dest_path|, because ModifyCacheState only moves files
  // if source and destination are different.
  *error = ModifyCacheState(target_path,  // source
                            target_path,  // destination
                            file_operation_type,
                            symlink_path,
                            true /* create symlink */);
}

void GDataCache::ClearDirty(const std::string& resource_id,
                            const std::string& md5,
                            FileOperationType file_operation_type,
                            GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  // |md5| is the new .<md5> extension to rename the file to.
  // So, search for entry in cache without comparing md5.
  GDataCacheEntry cache_entry;

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  if (!GetCacheEntry(resource_id, std::string(), &cache_entry) ||
      !cache_entry.is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << "res_id=" << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }

  // If a file is not dirty (it should have been marked dirty via
  // MarkDirtyInCache), clearing its dirty state is an invalid operation.
  if (!cache_entry.is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: res_id="
                 << resource_id
                 << ", md5=" << md5;
    *error = GDATA_FILE_ERROR_INVALID_OPERATION;
    return;
  }

  // File must be dirty and hence in persistent dir.
  DCHECK(cache_entry.is_persistent());

  // Get the current path of the file in cache.
  FilePath source_path = GetCacheFilePath(resource_id,
                                          md5,
                                          GetSubDirectoryType(cache_entry),
                                          CACHED_FILE_LOCALLY_MODIFIED);

  // Determine destination path.
  // If file is pinned, move it to persistent dir with .md5 extension;
  // otherwise, move it to tmp dir with .md5 extension.
  const CacheSubDirectoryType sub_dir_type =
      cache_entry.is_pinned() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
  FilePath dest_path = GetCacheFilePath(resource_id,
                                        md5,
                                        sub_dir_type,
                                        CACHED_FILE_FROM_SERVER);

  // Delete symlink in outgoing dir.
  FilePath symlink_path = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_OUTGOING,
                                           CACHED_FILE_FROM_SERVER);

  *error = ModifyCacheState(source_path,
                            dest_path,
                            file_operation_type,
                            symlink_path,
                            false /* don't create symlink */);

  // If file is pinned, update symlink in pinned dir.
  if (*error == GDATA_FILE_OK && cache_entry.is_pinned()) {
    symlink_path = GetCacheFilePath(resource_id,
                                    std::string(),
                                    CACHE_TYPE_PINNED,
                                    CACHED_FILE_FROM_SERVER);

    // Since there's no moving of files here, use |dest_path| for both
    // |source_path| and |dest_path|, because ModifyCacheState only moves files
    // if source and destination are different.
    *error = ModifyCacheState(dest_path,  // source path
                              dest_path,  // destination path
                              file_operation_type,
                              symlink_path,
                              true /* create symlink */);
  }

  if (*error == GDATA_FILE_OK) {
    // Now that file operations have completed, update cache map.
    cache_entry.set_md5(md5);
    cache_entry.set_is_dirty(false);
    cache_entry.set_is_persistent(sub_dir_type == CACHE_TYPE_PERSISTENT);
    metadata_->AddOrUpdateCacheEntry(resource_id, cache_entry);
  }
}

void GDataCache::Remove(const std::string& resource_id,
                        GDataFileError* error) {
  AssertOnSequencedWorkerPool();
  DCHECK(error);

  // MD5 is not passed into RemoveCacheEntry because we would delete all
  // cache files corresponding to <resource_id> regardless of the md5.
  // So, search for entry in cache without taking md5 into account.
  GDataCacheEntry cache_entry;

  // If entry doesn't exist or is dirty or mounted in cache, nothing to do.
  const bool entry_found =
      GetCacheEntry(resource_id, std::string(), &cache_entry);
  if (!entry_found || cache_entry.is_dirty() || cache_entry.is_mounted()) {
    DVLOG(1) << "Entry is "
             << (entry_found ?
                 (cache_entry.is_dirty() ? "dirty" : "mounted") :
                 "non-existent")
             << " in cache, not removing";
    *error = GDATA_FILE_OK;
    return;
  }

  // Determine paths to delete all cache versions of |resource_id| in
  // persistent, tmp and pinned directories.
  std::vector<FilePath> paths_to_delete;

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
  FilePath path_to_keep = GetCacheFilePath(resource_id,
                                           std::string(),
                                           CACHE_TYPE_PERSISTENT,
                                           CACHED_FILE_LOCALLY_MODIFIED);

  for (size_t i = 0; i < paths_to_delete.size(); ++i) {
    DeleteFilesSelectively(paths_to_delete[i], path_to_keep);
  }

  // Now that all file operations have completed, remove from cache map.
  metadata_->RemoveCacheEntry(resource_id);

  *error = GDATA_FILE_OK;
}

void GDataCache::OnPinned(GDataFileError* error,
                          const std::string& resource_id,
                          const std::string& md5,
                          const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);

  if (*error == GDATA_FILE_OK)
    FOR_EACH_OBSERVER(Observer, observers_, OnCachePinned(resource_id, md5));
}

void GDataCache::OnUnpinned(GDataFileError* error,
                            const std::string& resource_id,
                            const std::string& md5,
                            const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);

  if (*error == GDATA_FILE_OK)
    FOR_EACH_OBSERVER(Observer, observers_, OnCacheUnpinned(resource_id, md5));

  // Now the file is moved from "persistent" to "tmp" directory.
  // It's a chance to free up space if needed.
  bool* has_enough_space = new bool(false);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GDataCache::FreeDiskSpaceIfNeededFor,
                 base::Unretained(this),
                 0,
                 base::Owned(has_enough_space)));
}

void GDataCache::OnCommitDirty(GDataFileError* error,
                               const std::string& resource_id,
                               const std::string& md5,
                               const CacheOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error);

  if (!callback.is_null())
    callback.Run(*error, resource_id, md5);

  if (*error == GDATA_FILE_OK)
    FOR_EACH_OBSERVER(Observer, observers_, OnCacheCommitted(resource_id));
}

void GDataCache::GetCacheEntryHelper(const std::string& resource_id,
                                     const std::string& md5,
                                     bool* success,
                                     GDataCacheEntry* cache_entry) {
  AssertOnSequencedWorkerPool();
  DCHECK(success);
  DCHECK(cache_entry);

  *success = GetCacheEntry(resource_id, md5, cache_entry);
}

// static
FilePath GDataCache::GetCacheRootPath(Profile* profile) {
  FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  FilePath cache_root_path =
      cache_base_path.Append(chrome::kGDataCacheDirname);
  return cache_root_path.Append(kGDataCacheVersionDir);
}

// static
std::vector<FilePath> GDataCache::GetCachePaths(
    const FilePath& cache_root_path) {
  std::vector<FilePath> cache_paths;
  // The order should match GDataCache::CacheSubDirectoryType enum.
  cache_paths.push_back(cache_root_path.Append(kGDataCacheMetaDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCachePinnedDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheOutgoingDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCachePersistentDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDownloadsDir));
  cache_paths.push_back(cache_root_path.Append(kGDataCacheTmpDocumentsDir));
  return cache_paths;
}

// static
bool GDataCache::CreateCacheDirectories(
    const std::vector<FilePath>& paths_to_create) {
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
GDataCache::CacheSubDirectoryType GDataCache::GetSubDirectoryType(
    const GDataCacheEntry& cache_entry) {
  return cache_entry.is_persistent() ? CACHE_TYPE_PERSISTENT : CACHE_TYPE_TMP;
}

void SetFreeDiskSpaceGetterForTesting(FreeDiskSpaceGetterInterface* getter) {
  delete global_free_disk_getter_for_testing;  // Safe to delete NULL;
  global_free_disk_getter_for_testing = getter;
}

}  // namespace gdata
