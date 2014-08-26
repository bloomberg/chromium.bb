// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache.h"

#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Returns ID extracted from the path.
std::string GetIdFromPath(const base::FilePath& path) {
  return util::UnescapeCacheFileName(path.BaseName().AsUTF8Unsafe());
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
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
}

bool FileCache::IsUnderFileCacheDirectory(const base::FilePath& path) const {
  return cache_file_directory_.IsParent(path);
}

bool FileCache::FreeDiskSpaceIfNeededFor(int64 num_bytes) {
  AssertOnSequencedWorkerPool();

  // Do nothing and return if we have enough space.
  if (HasEnoughSpaceFor(num_bytes, cache_file_directory_))
    return true;

  // Otherwise, try to free up the disk space.
  DVLOG(1) << "Freeing up disk space for " << num_bytes;

  // Remove all entries unless specially marked.
  scoped_ptr<ResourceMetadataStorage::Iterator> it = storage_->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (it->GetValue().file_specific_info().has_cache_state() &&
        !it->GetValue().file_specific_info().cache_state().is_pinned() &&
        !it->GetValue().file_specific_info().cache_state().is_dirty() &&
        !mounted_files_.count(it->GetID())) {
      ResourceEntry entry(it->GetValue());
      entry.mutable_file_specific_info()->clear_cache_state();
      storage_->PutEntry(entry);
    }
  }
  if (it->HasError())
    return false;

  // Remove all files which have no corresponding cache entries.
  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  ResourceEntry entry;
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    std::string id = GetIdFromPath(current);
    FileError error = storage_->GetEntry(id, &entry);
    if (error == FILE_ERROR_NOT_FOUND ||
        (error == FILE_ERROR_OK &&
         !entry.file_specific_info().cache_state().is_present()))
      base::DeleteFile(current, false /* recursive */);
    else if (error != FILE_ERROR_OK)
      return false;
  }

  // Check the disk space again.
  return HasEnoughSpaceFor(num_bytes, cache_file_directory_);
}

FileError FileCache::GetFile(const std::string& id,
                             base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().cache_state().is_present())
    return FILE_ERROR_NOT_FOUND;

  *cache_file_path = GetCacheFilePath(id);
  return FILE_ERROR_OK;
}

FileError FileCache::Store(const std::string& id,
                           const std::string& md5,
                           const base::FilePath& source_path,
                           FileOperationType file_operation_type) {
  AssertOnSequencedWorkerPool();

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  int64 file_size = 0;
  if (file_operation_type == FILE_OPERATION_COPY) {
    if (!base::GetFileSize(source_path, &file_size)) {
      LOG(WARNING) << "Couldn't get file size for: " << source_path.value();
      return FILE_ERROR_FAILED;
    }
  }
  if (!FreeDiskSpaceIfNeededFor(file_size))
    return FILE_ERROR_NO_LOCAL_SPACE;

  // If file is mounted, return error.
  if (mounted_files_.count(id))
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
  FileCacheEntry* cache_state =
      entry.mutable_file_specific_info()->mutable_cache_state();
  cache_state->set_md5(md5);
  cache_state->set_is_present(true);
  if (md5.empty())
    cache_state->set_is_dirty(true);
  return storage_->PutEntry(entry);
}

FileError FileCache::Pin(const std::string& id) {
  AssertOnSequencedWorkerPool();

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  entry.mutable_file_specific_info()->mutable_cache_state()->set_is_pinned(
      true);
  return storage_->PutEntry(entry);
}

FileError FileCache::Unpin(const std::string& id) {
  AssertOnSequencedWorkerPool();

  // Unpinning a file means its entry must exist in cache.
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  // Now that file operations have completed, update metadata.
  if (entry.file_specific_info().cache_state().is_present()) {
    entry.mutable_file_specific_info()->mutable_cache_state()->set_is_pinned(
        false);
  } else {
    // Remove the existing entry if we are unpinning a non-present file.
    entry.mutable_file_specific_info()->clear_cache_state();
  }
  error = storage_->PutEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  // Now it's a chance to free up space if needed.
  FreeDiskSpaceIfNeededFor(0);

  return FILE_ERROR_OK;
}

FileError FileCache::MarkAsMounted(const std::string& id,
                                   base::FilePath* cache_file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(cache_file_path);

  // Get cache entry associated with the id and md5
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().cache_state().is_present())
    return FILE_ERROR_NOT_FOUND;

  if (mounted_files_.count(id))
    return FILE_ERROR_INVALID_OPERATION;

  // Ensure the file is readable to cros_disks. See crbug.com/236994.
  base::FilePath path = GetCacheFilePath(id);
  if (!base::SetPosixFilePermissions(
          path,
          base::FILE_PERMISSION_READ_BY_USER |
          base::FILE_PERMISSION_WRITE_BY_USER |
          base::FILE_PERMISSION_READ_BY_GROUP |
          base::FILE_PERMISSION_READ_BY_OTHERS))
    return FILE_ERROR_FAILED;

  mounted_files_.insert(id);

  *cache_file_path = path;
  return FILE_ERROR_OK;
}

FileError FileCache::OpenForWrite(
    const std::string& id,
    scoped_ptr<base::ScopedClosureRunner>* file_closer) {
  AssertOnSequencedWorkerPool();

  // Marking a file dirty means its entry and actual file blob must exist in
  // cache.
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().cache_state().is_present()) {
    LOG(WARNING) << "Can't mark dirty a file that wasn't cached: " << id;
    return FILE_ERROR_NOT_FOUND;
  }

  entry.mutable_file_specific_info()->mutable_cache_state()->set_is_dirty(true);
  entry.mutable_file_specific_info()->mutable_cache_state()->clear_md5();
  error = storage_->PutEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  write_opened_files_[id]++;
  file_closer->reset(new base::ScopedClosureRunner(
      base::Bind(&google_apis::RunTaskWithTaskRunner,
                 blocking_task_runner_,
                 base::Bind(&FileCache::CloseForWrite,
                            weak_ptr_factory_.GetWeakPtr(),
                            id))));
  return FILE_ERROR_OK;
}

bool FileCache::IsOpenedForWrite(const std::string& id) {
  AssertOnSequencedWorkerPool();
  return write_opened_files_.count(id);
}

FileError FileCache::UpdateMd5(const std::string& id) {
  AssertOnSequencedWorkerPool();

  if (IsOpenedForWrite(id))
    return FILE_ERROR_IN_USE;

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().cache_state().is_present())
    return FILE_ERROR_NOT_FOUND;

  const std::string& md5 = util::GetMd5Digest(GetCacheFilePath(id));
  if (md5.empty())
    return FILE_ERROR_NOT_FOUND;

  entry.mutable_file_specific_info()->mutable_cache_state()->set_md5(md5);
  return storage_->PutEntry(entry);
}

FileError FileCache::ClearDirty(const std::string& id) {
  AssertOnSequencedWorkerPool();

  if (IsOpenedForWrite(id))
    return FILE_ERROR_IN_USE;

  // Clearing a dirty file means its entry and actual file blob must exist in
  // cache.
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().cache_state().is_present()) {
    LOG(WARNING) << "Can't clear dirty state of a file that wasn't cached: "
                 << id;
    return FILE_ERROR_NOT_FOUND;
  }

  // If a file is not dirty (it should have been marked dirty via OpenForWrite),
  // clearing its dirty state is an invalid operation.
  if (!entry.file_specific_info().cache_state().is_dirty()) {
    LOG(WARNING) << "Can't clear dirty state of a non-dirty file: " << id;
    return FILE_ERROR_INVALID_OPERATION;
  }

  entry.mutable_file_specific_info()->mutable_cache_state()->set_is_dirty(
      false);
  return storage_->PutEntry(entry);
}

FileError FileCache::Remove(const std::string& id) {
  AssertOnSequencedWorkerPool();

  ResourceEntry entry;

  // If entry doesn't exist, nothing to do.
  FileError error = storage_->GetEntry(id, &entry);
  if (error == FILE_ERROR_NOT_FOUND)
    return FILE_ERROR_OK;
  if (error != FILE_ERROR_OK)
    return error;
  if (!entry.file_specific_info().has_cache_state())
    return FILE_ERROR_OK;

  // Cannot delete a mounted file.
  if (mounted_files_.count(id))
    return FILE_ERROR_IN_USE;

  // Delete the file.
  base::FilePath path = GetCacheFilePath(id);
  if (!base::DeleteFile(path, false /* recursive */))
    return FILE_ERROR_FAILED;

  // Now that all file operations have completed, remove from metadata.
  entry.mutable_file_specific_info()->clear_cache_state();
  return storage_->PutEntry(entry);
}

bool FileCache::ClearAll() {
  AssertOnSequencedWorkerPool();

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

  // Older versions do not clear MD5 when marking entries dirty.
  // Clear MD5 of all dirty entries to deal with old data.
  scoped_ptr<ResourceMetadataStorage::Iterator> it = storage_->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (it->GetValue().file_specific_info().cache_state().is_dirty()) {
      ResourceEntry new_entry(it->GetValue());
      new_entry.mutable_file_specific_info()->mutable_cache_state()->
          clear_md5();
      if (storage_->PutEntry(new_entry) != FILE_ERROR_OK)
        return false;
    }
  }
  if (it->HasError())
    return false;

  if (!RenameCacheFilesToNewFormat())
    return false;
  return true;
}

void FileCache::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

bool FileCache::RecoverFilesFromCacheDirectory(
    const base::FilePath& dest_directory,
    const ResourceMetadataStorage::RecoveredCacheInfoMap&
        recovered_cache_info) {
  int file_number = 1;

  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    const std::string& id = GetIdFromPath(current);
    ResourceEntry entry;
    FileError error = storage_->GetEntry(id, &entry);
    if (error != FILE_ERROR_OK && error != FILE_ERROR_NOT_FOUND)
      return false;
    if (error == FILE_ERROR_OK &&
        entry.file_specific_info().cache_state().is_present()) {
      // This file is managed by FileCache, no need to recover it.
      continue;
    }

    // If a cache entry which is non-dirty and has matching MD5 is found in
    // |recovered_cache_entries|, it means the current file is already uploaded
    // to the server. Just delete it instead of recovering it.
    ResourceMetadataStorage::RecoveredCacheInfoMap::const_iterator it =
        recovered_cache_info.find(id);
    if (it != recovered_cache_info.end()) {
      // Due to the DB corruption, cache info might be recovered from old
      // revision. Perform MD5 check even when is_dirty is false just in case.
      if (!it->second.is_dirty &&
          it->second.md5 == util::GetMd5Digest(current)) {
        base::DeleteFile(current, false /* recursive */);
        continue;
      }
    }

    // Read file contents to sniff mime type.
    std::vector<char> content(net::kMaxBytesToSniff);
    const int read_result =
        base::ReadFile(current, &content[0], content.size());
    if (read_result < 0) {
      LOG(WARNING) << "Cannot read: " << current.value();
      return false;
    }
    if (read_result == 0)  // Skip empty files.
      continue;

    // Use recovered file name if available, otherwise decide file name with
    // sniffed mime type.
    base::FilePath dest_base_name(FILE_PATH_LITERAL("file"));
    std::string mime_type;
    if (it != recovered_cache_info.end() && !it->second.title.empty()) {
      // We can use a file name recovered from the trashed DB.
      dest_base_name = base::FilePath::FromUTF8Unsafe(it->second.title);
    } else if (net::SniffMimeType(&content[0], read_result,
                                  net::FilePathToFileURL(current),
                                  std::string(), &mime_type) ||
               net::SniffMimeTypeFromLocalData(&content[0], read_result,
                                               &mime_type)) {
      // Change base name for common mime types.
      if (net::MatchesMimeType("image/*", mime_type)) {
        dest_base_name = base::FilePath(FILE_PATH_LITERAL("image"));
      } else if (net::MatchesMimeType("video/*", mime_type)) {
        dest_base_name = base::FilePath(FILE_PATH_LITERAL("video"));
      } else if (net::MatchesMimeType("audio/*", mime_type)) {
        dest_base_name = base::FilePath(FILE_PATH_LITERAL("audio"));
      }

      // Estimate extension from mime type.
      std::vector<base::FilePath::StringType> extensions;
      base::FilePath::StringType extension;
      if (net::GetPreferredExtensionForMimeType(mime_type, &extension))
        extensions.push_back(extension);
      else
        net::GetExtensionsForMimeType(mime_type, &extensions);

      // Add extension if possible.
      if (!extensions.empty())
        dest_base_name = dest_base_name.AddExtension(extensions[0]);
    }

    // Add file number to the file name and move.
    const base::FilePath& dest_path = dest_directory.Append(dest_base_name)
        .InsertBeforeExtensionASCII(base::StringPrintf("%08d", file_number++));
    if (!base::CreateDirectory(dest_directory) ||
        !base::Move(current, dest_path)) {
      LOG(WARNING) << "Failed to move: " << current.value()
                   << " to " << dest_path.value();
      return false;
    }
  }
  UMA_HISTOGRAM_COUNTS("Drive.NumberOfCacheFilesRecoveredAfterDBCorruption",
                       file_number - 1);
  return true;
}

FileError FileCache::MarkAsUnmounted(const base::FilePath& file_path) {
  AssertOnSequencedWorkerPool();
  DCHECK(IsUnderFileCacheDirectory(file_path));

  std::string id = GetIdFromPath(file_path);

  // Get the entry associated with the id.
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

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
  free_space -= cryptohome::kMinFreeSpaceInBytes;
  return (free_space >= num_bytes);
}

bool FileCache::RenameCacheFilesToNewFormat() {
  base::FileEnumerator enumerator(cache_file_directory_,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    base::FilePath new_path = current.RemoveExtension();
    if (!new_path.Extension().empty()) {
      // Delete files with multiple extensions.
      if (!base::DeleteFile(current, false /* recursive */))
        return false;
      continue;
    }
    const std::string& id = GetIdFromPath(new_path);
    new_path = GetCacheFilePath(util::CanonicalizeResourceId(id));
    if (new_path != current && !base::Move(current, new_path))
      return false;
  }
  return true;
}

void FileCache::CloseForWrite(const std::string& id) {
  AssertOnSequencedWorkerPool();

  std::map<std::string, int>::iterator it = write_opened_files_.find(id);
  if (it == write_opened_files_.end())
    return;

  DCHECK_LT(0, it->second);
  --it->second;
  if (it->second == 0)
    write_opened_files_.erase(it);

  // Update last modified date.
  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to get entry: " << id << ", "
               << FileErrorToString(error);
    return;
  }
  entry.mutable_file_info()->set_last_modified(
      base::Time::Now().ToInternalValue());
  error = storage_->PutEntry(entry);
  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to put entry: " << id << ", "
               << FileErrorToString(error);
  }
}

}  // namespace internal
}  // namespace drive
