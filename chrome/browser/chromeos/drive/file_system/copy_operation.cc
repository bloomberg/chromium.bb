// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include <string>

#include "base/file_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

int64 GetFileSize(const base::FilePath& file_path) {
  int64 file_size;
  if (!file_util::GetFileSize(file_path, &file_size))
    return -1;
  return file_size;
}

// Stores the file at |local_file_path| to the cache as a content of entry at
// |remote_dest_path|, and marks it dirty.
FileError UpdateLocalStateForScheduleTransfer(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    std::string* resource_id) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryByPath(remote_dest_path, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = cache->Store(
      entry.resource_id(), entry.file_specific_info().md5(), local_src_path,
      internal::FileCache::FILE_OPERATION_COPY);
  if (error != FILE_ERROR_OK)
    return error;

  error = cache->MarkDirty(entry.resource_id());
  if (error != FILE_ERROR_OK)
    return error;

  *resource_id = entry.resource_id();
  return FILE_ERROR_OK;
}

// Gets the file size of the |local_path|, and the ResourceEntry for the parent
// of |remote_path| to prepare the necessary information for transfer.
FileError PrepareTransferFileFromLocalToRemote(
    internal::ResourceMetadata* metadata,
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    std::string* resource_id) {
  ResourceEntry parent_entry;
  FileError error = metadata->GetResourceEntryByPath(
      remote_dest_path.DirName(), &parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  // The destination's parent must be a directory.
  if (!parent_entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // Try to parse GDoc File and extract the resource id, if necessary.
  // Failing isn't problem. It'd be handled as a regular file, then.
  if (util::HasGDocFileExtension(local_src_path))
    *resource_id = util::ReadResourceIdFromGDocFile(local_src_path);

  return FILE_ERROR_OK;
}

FileError AddEntryToLocalMetadata(internal::ResourceMetadata* metadata,
                                  const ResourceEntry& entry) {
  FileError error = metadata->AddEntry(entry);
  // Depending on timing, the metadata may have inserted via change list
  // already. So, FILE_ERROR_EXISTS is not an error.
  if (error == FILE_ERROR_EXISTS)
    error = FILE_ERROR_OK;
  return error;
}

}  // namespace

CopyOperation::CopyOperation(base::SequencedTaskRunner* blocking_task_runner,
                             OperationObserver* observer,
                             JobScheduler* scheduler,
                             internal::ResourceMetadata* metadata,
                             internal::FileCache* cache,
                             DriveServiceInterface* drive_service,
                             const base::FilePath& temporary_file_directory)
  : blocking_task_runner_(blocking_task_runner),
    observer_(observer),
    scheduler_(scheduler),
    metadata_(metadata),
    cache_(cache),
    drive_service_(drive_service),
    create_file_operation_(new CreateFileOperation(blocking_task_runner,
                                                   observer,
                                                   scheduler,
                                                   metadata,
                                                   cache)),
    download_operation_(new DownloadOperation(blocking_task_runner,
                                              observer,
                                              scheduler,
                                              metadata,
                                              cache,
                                              temporary_file_directory)),
    move_operation_(new MoveOperation(blocking_task_runner,
                                      observer,
                                      scheduler,
                                      metadata)),
    weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CopyOperation::~CopyOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CopyOperation::Copy(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const FileOperationCallback& callback) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  DCHECK(!callback.is_null());

  metadata_->GetResourceEntryPairByPathsOnUIThread(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&CopyOperation::CopyAfterGetResourceEntryPair,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void CopyOperation::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* resource_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &PrepareTransferFileFromLocalToRemote,
          metadata_, local_src_path, remote_dest_path, resource_id),
      base::Bind(
          &CopyOperation::TransferFileFromLocalToRemoteAfterPrepare,
          weak_ptr_factory_.GetWeakPtr(),
          local_src_path, remote_dest_path, callback,
          base::Owned(resource_id)));
}

void CopyOperation::TransferFileFromLocalToRemoteAfterPrepare(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    std::string* resource_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // For regular files, schedule the transfer.
  if (resource_id->empty()) {
    ScheduleTransferRegularFile(local_src_path, remote_dest_path, callback);
    return;
  }

  // This is uploading a JSON file representing a hosted document.
  // Copy the document on the Drive server.

  // GDoc file may contain a resource ID in the old format.
  const std::string canonicalized_resource_id =
      drive_service_->CanonicalizeResourceId(*resource_id);

  // TODO(hidehiko): Use CopyResource for Drive API v2.

  CopyHostedDocumentToDirectory(
      // Drop the document extension, which should not be
      // in the document title.
      // TODO(yoshiki): Remove this code with crbug.com/223304.
      remote_dest_path.RemoveExtension(),
      canonicalized_resource_id,
      callback);
}

void CopyOperation::ScheduleTransferRegularFile(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&GetFileSize, local_src_path),
      base::Bind(&CopyOperation::ScheduleTransferRegularFileAfterGetFileSize,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_src_path, remote_dest_path, callback));
}

void CopyOperation::ScheduleTransferRegularFileAfterGetFileSize(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    int64 local_file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (local_file_size < 0) {
    callback.Run(FILE_ERROR_NOT_FOUND);
    return;
  }

  // For regular files, check the server-side quota whether sufficient space
  // is available for the file to be uploaded.
  scheduler_->GetAboutResource(
      base::Bind(
          &CopyOperation::ScheduleTransferRegularFileAfterGetAboutResource,
          weak_ptr_factory_.GetWeakPtr(),
          local_src_path, remote_dest_path, callback, local_file_size));
}

void CopyOperation::ScheduleTransferRegularFileAfterGetAboutResource(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    int64 local_file_size,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(about_resource);
  const int64 space =
      about_resource->quota_bytes_total() - about_resource->quota_bytes_used();
  if (space < local_file_size) {
    callback.Run(FILE_ERROR_NO_SERVER_SPACE);
    return;
  }

  create_file_operation_->CreateFile(
      remote_dest_path,
      true,  // Exclusive (i.e. fail if a file already exists).
      std::string(),  // no specific mime type; CreateFile should guess it.
      base::Bind(&CopyOperation::ScheduleTransferRegularFileAfterCreate,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_src_path, remote_dest_path, callback));
}

void CopyOperation::ScheduleTransferRegularFileAfterCreate(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  std::string* resource_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &UpdateLocalStateForScheduleTransfer,
          metadata_, cache_, local_src_path, remote_dest_path, resource_id),
      base::Bind(
          &CopyOperation::ScheduleTransferRegularFileAfterUpdateLocalState,
          weak_ptr_factory_.GetWeakPtr(), callback, base::Owned(resource_id)));
}

void CopyOperation::ScheduleTransferRegularFileAfterUpdateLocalState(
    const FileOperationCallback& callback,
    std::string* resource_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    observer_->OnCacheFileUploadNeededByOperation(*resource_id);
  callback.Run(error);
}

void CopyOperation::CopyHostedDocumentToDirectory(
    const base::FilePath& dest_path,
    const std::string& resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->CopyHostedDocument(
      resource_id,
      dest_path.BaseName().AsUTF8Unsafe(),
      base::Bind(&CopyOperation::OnCopyHostedDocumentCompleted,
                 weak_ptr_factory_.GetWeakPtr(), dest_path, callback));
}

void CopyOperation::OnCopyHostedDocumentCompleted(
    const base::FilePath& dest_path,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(resource_entry);

  ResourceEntry entry;
  if (!ConvertToResourceEntry(*resource_entry, &entry)) {
    callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  // The entry was added in the root directory on the server, so we should
  // first add it to the root to mirror the state and then move it to the
  // destination directory by MoveEntryFromRootDirectory().
  metadata_->AddEntryOnUIThread(
      entry,
      base::Bind(&CopyOperation::MoveEntryFromRootDirectory,
                 weak_ptr_factory_.GetWeakPtr(), dest_path, callback));
}

void CopyOperation::MoveEntryFromRootDirectory(
    const base::FilePath& dest_path,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Depending on timing, the metadata may have inserted via change list
  // already. So, FILE_ERROR_EXISTS is not an error.
  if (error == FILE_ERROR_EXISTS)
    error = FILE_ERROR_OK;

  // Return if there is an error or |dir_path| is the root directory.
  if (error != FILE_ERROR_OK ||
      dest_path.DirName() == util::GetDriveMyDriveRootPath()) {
    callback.Run(error);
    return;
  }

  DCHECK_EQ(util::GetDriveMyDriveRootPath().value(),
            file_path.DirName().value()) << file_path.value();

  move_operation_->Move(file_path, dest_path, callback);
}

void CopyOperation::CopyAfterGetResourceEntryPair(
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != FILE_ERROR_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != FILE_ERROR_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<ResourceEntry> src_file_proto = result->first.entry.Pass();
  scoped_ptr<ResourceEntry> dest_parent_proto = result->second.entry.Pass();

  if (!dest_parent_proto->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  } else if (src_file_proto->file_info().is_directory()) {
    // TODO(kochi): Implement copy for directories. In the interim,
    // we handle recursive directory copy in the file manager.
    // crbug.com/141596
    callback.Run(FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // If Drive API v2 is enabled, we can copy resources on server side.
  if (util::IsDriveV2ApiEnabled()) {
    base::FilePath new_title = dest_file_path.BaseName();
    if (src_file_proto->file_specific_info().is_hosted_document()) {
      // Drop the document extension, which should not be in the title.
      // TODO(yoshiki): Remove this code with crbug.com/223304.
      new_title = new_title.RemoveExtension();
    }

    scheduler_->CopyResource(
        src_file_proto->resource_id(),
        dest_parent_proto->resource_id(),
        new_title.value(),
        base::Bind(&CopyOperation::OnCopyResourceCompleted,
                   weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }


  if (src_file_proto->file_specific_info().is_hosted_document()) {
    CopyHostedDocumentToDirectory(
        // Drop the document extension, which should not be in the title.
        // TODO(yoshiki): Remove this code with crbug.com/223304.
        dest_file_path.RemoveExtension(),
        src_file_proto->resource_id(),
        callback);
    return;
  }

  const base::FilePath& src_file_path = result->first.path;
  download_operation_->EnsureFileDownloadedByPath(
      src_file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(&CopyOperation::OnGetFileCompleteForCopy,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void CopyOperation::OnCopyResourceCompleted(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(resource_entry);
  ResourceEntry entry;
  if (!ConvertToResourceEntry(*resource_entry, &entry)) {
    callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  // The copy on the server side is completed successfully. Update the local
  // metadata.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&AddEntryToLocalMetadata, metadata_, entry),
      callback);
}

void CopyOperation::OnGetFileCompleteForCopy(
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // This callback is only triggered for a regular file via Copy().
  DCHECK(entry && !entry->file_specific_info().is_hosted_document());
  ScheduleTransferRegularFile(local_file_path, remote_dest_path, callback);
}

}  // namespace file_system
}  // namespace drive
