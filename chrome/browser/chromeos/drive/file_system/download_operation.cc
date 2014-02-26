// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/download_operation.h"

#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/gdata_errorcode.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Generates an unused file path with |extension| to |out_path|, as a descendant
// of |dir|, with its parent directory created.
bool GeneratesUniquePathWithExtension(
    const base::FilePath& dir,
    const base::FilePath::StringType& extension,
    base::FilePath* out_path) {
  base::FilePath subdir;
  if (!base::CreateTemporaryDirInDir(dir, base::FilePath::StringType(),
                                     &subdir)) {
    return false;
  }
  *out_path = subdir.Append(FILE_PATH_LITERAL("tmp") + extension);
  return true;
}

// If the resource is a hosted document, creates a JSON file representing the
// resource locally, and returns FILE_ERROR_OK with |cache_file_path| storing
// the path to the JSON file.
// If the resource is a regular file and its local cache is available,
// returns FILE_ERROR_OK with |cache_file_path| storing the path to the
// cache file.
// If the resource is a regular file but its local cache is NOT available,
// returns FILE_ERROR_OK, but |cache_file_path| is kept empty.
// Otherwise returns error code.
FileError CheckPreConditionForEnsureFileDownloaded(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& temporary_file_directory,
    const std::string& local_id,
    ResourceEntry* entry,
    base::FilePath* cache_file_path) {
  DCHECK(metadata);
  DCHECK(cache);
  DCHECK(cache_file_path);

  FileError error = metadata->GetResourceEntryById(local_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (entry->file_info().is_directory())
    return FILE_ERROR_NOT_A_FILE;

  // For a hosted document, we create a special JSON file to represent the
  // document instead of fetching the document content in one of the exported
  // formats. The JSON file contains the edit URL and resource ID of the
  // document.
  if (entry->file_specific_info().is_hosted_document()) {
    base::FilePath::StringType extension = base::FilePath::FromUTF8Unsafe(
        entry->file_specific_info().document_extension()).value();
    base::FilePath gdoc_file_path;
    // TODO(rvargas): Convert this code to use base::File::Info.
    base::File::Info file_info;
    // We add the gdoc file extension in the temporary file, so that in cross
    // profile drag-and-drop between Drive folders, the destination profiles's
    // CopyOperation can detect the special JSON file only by the path.
    if (!GeneratesUniquePathWithExtension(temporary_file_directory,
                                          extension,
                                          &gdoc_file_path) ||
        !util::CreateGDocFile(gdoc_file_path,
                              GURL(entry->file_specific_info().alternate_url()),
                              entry->resource_id()) ||
        !base::GetFileInfo(gdoc_file_path,
                           reinterpret_cast<base::File::Info*>(&file_info)))
      return FILE_ERROR_FAILED;

    *cache_file_path = gdoc_file_path;
    SetPlatformFileInfoToResourceEntry(file_info, entry);
    return FILE_ERROR_OK;
  }

  FileCacheEntry cache_entry;
  if (!cache->GetCacheEntry(local_id, &cache_entry) ||
      !cache_entry.is_present()) {  // This file has no cache file.
    if (!entry->resource_id().empty()) {
      // This entry exists on the server, leave |cache_file_path| empty to
      // start download.
      return FILE_ERROR_OK;
    }

    // This entry does not exist on the server, store an empty file and mark it
    // as dirty.
    base::FilePath empty_file;
    if (!base::CreateTemporaryFileInDir(temporary_file_directory, &empty_file))
      return FILE_ERROR_FAILED;
    error = cache->Store(local_id, std::string(), empty_file,
                         internal::FileCache::FILE_OPERATION_MOVE);
    if (error != FILE_ERROR_OK)
      return error;

    if (!cache->GetCacheEntry(local_id, &cache_entry))
      return FILE_ERROR_NOT_FOUND;
  }

  // Leave |cache_file_path| empty when the stored file is obsolete and has no
  // local modification.
  if (!cache_entry.is_dirty() &&
      entry->file_specific_info().md5() != cache_entry.md5())
    return FILE_ERROR_OK;

  // Fill |cache_file_path| with the path to the cached file.
  error = cache->GetFile(local_id, cache_file_path);
  if (error != FILE_ERROR_OK)
    return error;

  // If the cache file is to be returned as the download result, the file info
  // of the cache needs to be returned via |entry|.
  // TODO(kinaba): crbug.com/246469. The logic below is similar to that in
  // drive::FileSystem::CheckLocalModificationAndRun. We should merge them.
  base::File::Info file_info;
  if (base::GetFileInfo(*cache_file_path, &file_info))
    SetPlatformFileInfoToResourceEntry(file_info, entry);

  return FILE_ERROR_OK;
}

// Calls CheckPreConditionForEnsureFileDownloaded() with the entry specified by
// the given ID. Also fills |drive_file_path| with the path of the entry.
FileError CheckPreConditionForEnsureFileDownloadedByLocalId(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const std::string& local_id,
    const base::FilePath& temporary_file_directory,
    base::FilePath* drive_file_path,
    base::FilePath* cache_file_path,
    ResourceEntry* entry) {
  *drive_file_path = metadata->GetFilePath(local_id);
  return CheckPreConditionForEnsureFileDownloaded(
      metadata, cache, temporary_file_directory, local_id, entry,
      cache_file_path);
}

// Calls CheckPreConditionForEnsureFileDownloaded() with the entry specified by
// the given file path.
FileError CheckPreConditionForEnsureFileDownloadedByPath(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& file_path,
    const base::FilePath& temporary_file_directory,
    base::FilePath* cache_file_path,
    ResourceEntry* entry) {
  std::string local_id;
  FileError error = metadata->GetIdByPath(file_path, &local_id);
  if (error != FILE_ERROR_OK)
    return error;
  return CheckPreConditionForEnsureFileDownloaded(
      metadata, cache, temporary_file_directory, local_id, entry,
      cache_file_path);
}

// Creates a file with unique name in |dir| and stores the path to |temp_file|.
// Additionally, sets the permission of the file to allow read access from
// others and group member users (i.e, "-rw-r--r--").
// We need this wrapper because Drive cache files may be read from other
// processes (e.g., cros_disks for mounting zip files).
bool CreateTemporaryReadableFileInDir(const base::FilePath& dir,
                                      base::FilePath* temp_file) {
  if (!base::CreateTemporaryFileInDir(dir, temp_file))
    return false;
  return base::SetPosixFilePermissions(
      *temp_file,
      base::FILE_PERMISSION_READ_BY_USER |
      base::FILE_PERMISSION_WRITE_BY_USER |
      base::FILE_PERMISSION_READ_BY_GROUP |
      base::FILE_PERMISSION_READ_BY_OTHERS);
}

// Prepares for downloading the file. Allocates the enough space for the file
// in the cache.
// If succeeded, returns FILE_ERROR_OK with |temp_download_file| storing the
// path to the file in the cache.
FileError PrepareForDownloadFile(internal::FileCache* cache,
                                 int64 expected_file_size,
                                 const base::FilePath& temporary_file_directory,
                                 base::FilePath* temp_download_file) {
  DCHECK(cache);
  DCHECK(temp_download_file);

  // Ensure enough space in the cache.
  if (!cache->FreeDiskSpaceIfNeededFor(expected_file_size))
    return FILE_ERROR_NO_LOCAL_SPACE;

  // Create the temporary file which will store the downloaded content.
  return CreateTemporaryReadableFileInDir(
      temporary_file_directory,
      temp_download_file) ? FILE_ERROR_OK : FILE_ERROR_FAILED;
}

// Stores the downloaded file at |downloaded_file_path| into |cache|.
// If succeeded, returns FILE_ERROR_OK with |cache_file_path| storing the
// path to the cache file.
// If failed, returns an error code with deleting |downloaded_file_path|.
FileError UpdateLocalStateForDownloadFile(
    internal::FileCache* cache,
    const std::string& local_id,
    const std::string& md5,
    google_apis::GDataErrorCode gdata_error,
    const base::FilePath& downloaded_file_path,
    base::FilePath* cache_file_path) {
  DCHECK(cache);

  // Downloaded file should be deleted on errors.
  base::ScopedClosureRunner file_deleter(base::Bind(
      base::IgnoreResult(&base::DeleteFile),
      downloaded_file_path, false /* recursive */));

  FileError error = GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK)
    return error;

  // Do not overwrite locally edited file with server side contents.
  FileCacheEntry cache_entry;
  if (cache->GetCacheEntry(local_id, &cache_entry) && cache_entry.is_dirty())
    return FILE_ERROR_IN_USE;

  // Here the download is completed successfully, so store it into the cache.
  error = cache->Store(local_id, md5, downloaded_file_path,
                       internal::FileCache::FILE_OPERATION_MOVE);
  if (error != FILE_ERROR_OK)
    return error;
  base::Closure unused_file_deleter_closure = file_deleter.Release();

  return cache->GetFile(local_id, cache_file_path);
}

}  // namespace

class DownloadOperation::DownloadParams {
 public:
  DownloadParams(
      const GetFileContentInitializedCallback initialized_callback,
      const google_apis::GetContentCallback get_content_callback,
      const GetFileCallback completion_callback,
      scoped_ptr<ResourceEntry> entry)
      : initialized_callback_(initialized_callback),
        get_content_callback_(get_content_callback),
        completion_callback_(completion_callback),
        entry_(entry.Pass()) {
    DCHECK(!completion_callback_.is_null());
    DCHECK(entry_);
  }

  void OnCacheFileFound(const base::FilePath& cache_file_path) const {
    if (initialized_callback_.is_null())
      return;

    DCHECK(entry_);
    initialized_callback_.Run(
        FILE_ERROR_OK, make_scoped_ptr(new ResourceEntry(*entry_)),
        cache_file_path, base::Closure());
  }

  void OnStartDownloading(const base::Closure& cancel_download_closure) const {
    if (initialized_callback_.is_null()) {
      return;
    }

    DCHECK(entry_);
    initialized_callback_.Run(
        FILE_ERROR_OK, make_scoped_ptr(new ResourceEntry(*entry_)),
        base::FilePath(), cancel_download_closure);
  }

  void OnError(FileError error) const {
    completion_callback_.Run(
        error, base::FilePath(), scoped_ptr<ResourceEntry>());
  }

  void OnComplete(const base::FilePath& cache_file_path) {
    completion_callback_.Run(FILE_ERROR_OK, cache_file_path, entry_.Pass());
  }

  const google_apis::GetContentCallback& get_content_callback() const {
    return get_content_callback_;
  }

  const ResourceEntry& entry() const { return *entry_; }

 private:
  const GetFileContentInitializedCallback initialized_callback_;
  const google_apis::GetContentCallback get_content_callback_;
  const GetFileCallback completion_callback_;

  scoped_ptr<ResourceEntry> entry_;

  DISALLOW_COPY_AND_ASSIGN(DownloadParams);
};

DownloadOperation::DownloadOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& temporary_file_directory)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      cache_(cache),
      temporary_file_directory_(temporary_file_directory),
      weak_ptr_factory_(this) {
}

DownloadOperation::~DownloadOperation() {
}

void DownloadOperation::EnsureFileDownloadedByLocalId(
    const std::string& local_id,
    const ClientContext& context,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const GetFileCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!completion_callback.is_null());

  base::FilePath* drive_file_path = new base::FilePath;
  base::FilePath* cache_file_path = new base::FilePath;
  ResourceEntry* entry = new ResourceEntry;
  scoped_ptr<DownloadParams> params(new DownloadParams(
      initialized_callback, get_content_callback, completion_callback,
      make_scoped_ptr(entry)));
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckPreConditionForEnsureFileDownloadedByLocalId,
                 base::Unretained(metadata_),
                 base::Unretained(cache_),
                 local_id,
                 temporary_file_directory_,
                 drive_file_path,
                 cache_file_path,
                 entry),
      base::Bind(&DownloadOperation::EnsureFileDownloadedAfterCheckPreCondition,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 context,
                 base::Owned(drive_file_path),
                 base::Owned(cache_file_path)));
}

void DownloadOperation::EnsureFileDownloadedByPath(
    const base::FilePath& file_path,
    const ClientContext& context,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const GetFileCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!completion_callback.is_null());

  base::FilePath* drive_file_path = new base::FilePath(file_path);
  base::FilePath* cache_file_path = new base::FilePath;
  ResourceEntry* entry = new ResourceEntry;
  scoped_ptr<DownloadParams> params(new DownloadParams(
      initialized_callback, get_content_callback, completion_callback,
      make_scoped_ptr(entry)));
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckPreConditionForEnsureFileDownloadedByPath,
                 base::Unretained(metadata_),
                 base::Unretained(cache_),
                 file_path,
                 temporary_file_directory_,
                 cache_file_path,
                 entry),
      base::Bind(&DownloadOperation::EnsureFileDownloadedAfterCheckPreCondition,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 context,
                 base::Owned(drive_file_path),
                 base::Owned(cache_file_path)));
}

void DownloadOperation::EnsureFileDownloadedAfterCheckPreCondition(
    scoped_ptr<DownloadParams> params,
    const ClientContext& context,
    base::FilePath* drive_file_path,
    base::FilePath* cache_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);
  DCHECK(drive_file_path);
  DCHECK(cache_file_path);

  if (error != FILE_ERROR_OK) {
    // During precondition check, an error is found.
    params->OnError(error);
    return;
  }

  if (!cache_file_path->empty()) {
    // The cache file is found.
    params->OnCacheFileFound(*cache_file_path);
    params->OnComplete(*cache_file_path);
    return;
  }

  DCHECK(!params->entry().resource_id().empty());

  // If cache file is not found, try to download the file from the server
  // instead. Check if we have enough space, based on the expected file size.
  // - if we don't have enough space, try to free up the disk space
  // - if we still don't have enough space, return "no space" error
  // - if we have enough space, start downloading the file from the server
  int64 size = params->entry().file_info().size();
  base::FilePath* temp_download_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareForDownloadFile,
                 base::Unretained(cache_),
                 size,
                 temporary_file_directory_,
                 temp_download_file_path),
      base::Bind(
          &DownloadOperation::EnsureFileDownloadedAfterPrepareForDownloadFile,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&params),
          context,
          *drive_file_path,
          base::Owned(temp_download_file_path)));
}

void DownloadOperation::EnsureFileDownloadedAfterPrepareForDownloadFile(
    scoped_ptr<DownloadParams> params,
    const ClientContext& context,
    const base::FilePath& drive_file_path,
    base::FilePath* temp_download_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);
  DCHECK(temp_download_file_path);

  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  DownloadParams* params_ptr = params.get();
  JobID id = scheduler_->DownloadFile(
      drive_file_path,
      params_ptr->entry().file_info().size(),
      *temp_download_file_path,
      params_ptr->entry().resource_id(),
      context,
      base::Bind(&DownloadOperation::EnsureFileDownloadedAfterDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 drive_file_path,
                 base::Passed(&params)),
      params_ptr->get_content_callback());

  // Notify via |initialized_callback| if necessary.
  params_ptr->OnStartDownloading(
      base::Bind(&DownloadOperation::CancelJob,
                 weak_ptr_factory_.GetWeakPtr(), id));
}

void DownloadOperation::EnsureFileDownloadedAfterDownloadFile(
    const base::FilePath& drive_file_path,
    scoped_ptr<DownloadParams> params,
    google_apis::GDataErrorCode gdata_error,
    const base::FilePath& downloaded_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& local_id = params->entry().local_id();
  const std::string& md5 = params->entry().file_specific_info().md5();
  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalStateForDownloadFile,
                 base::Unretained(cache_),
                 local_id,
                 md5,
                 gdata_error,
                 downloaded_file_path,
                 cache_file_path),
      base::Bind(&DownloadOperation::EnsureFileDownloadedAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 drive_file_path,
                 base::Passed(&params),
                 base::Owned(cache_file_path)));
}

void DownloadOperation::EnsureFileDownloadedAfterUpdateLocalState(
    const base::FilePath& file_path,
    scoped_ptr<DownloadParams> params,
    base::FilePath* cache_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  // Storing to cache changes the "offline available" status, hence notify.
  observer_->OnDirectoryChangedByOperation(file_path.DirName());
  params->OnComplete(*cache_file_path);
}

void DownloadOperation::CancelJob(JobID job_id) {
  scheduler_->CancelJob(job_id);
}

}  // namespace file_system
}  // namespace drive
