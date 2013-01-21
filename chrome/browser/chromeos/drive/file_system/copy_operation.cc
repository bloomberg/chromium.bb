// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include <string>

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/google_apis/drive_upload_error.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;
using google_apis::ResourceEntry;
using google_apis::GDataErrorCode;

namespace drive {
namespace file_system {

namespace {

const char kMimeTypeOctetStream[] = "application/octet-stream";

// Copies a file from |src_file_path| to |dest_file_path| on the local
// file system using file_util::CopyFile.
// Returns DRIVE_FILE_OK on success or DRIVE_FILE_ERROR_FAILED otherwise.
DriveFileError CopyLocalFileOnBlockingPool(const FilePath& src_file_path,
                                           const FilePath& dest_file_path) {
  return file_util::CopyFile(src_file_path, dest_file_path) ?
      DRIVE_FILE_OK : DRIVE_FILE_ERROR_FAILED;
}

// Checks if a local file at |local_file_path| is a JSON file referencing a
// hosted document on blocking pool, and if so, gets the resource ID of the
// document.
std::string GetDocumentResourceIdOnBlockingPool(
    const FilePath& local_file_path) {
  std::string result;
  if (ResourceEntry::HasHostedDocumentExtension(local_file_path)) {
    std::string error;
    DictionaryValue* dict_value = NULL;
    JSONFileValueSerializer serializer(local_file_path);
    scoped_ptr<Value> value(serializer.Deserialize(NULL, &error));
    if (value.get() && value->GetAsDictionary(&dict_value))
      dict_value->GetString("resource_id", &result);
  }
  return result;
}

}  // namespace

// CopyOperation::StartFileUploadParams implementation.
struct CopyOperation::StartFileUploadParams {
  StartFileUploadParams(const FilePath& in_local_file_path,
                        const FilePath& in_remote_file_path,
                        const FileOperationCallback& in_callback)
      : local_file_path(in_local_file_path),
        remote_file_path(in_remote_file_path),
        callback(in_callback) {}

  const FilePath local_file_path;
  const FilePath remote_file_path;
  const FileOperationCallback callback;
};

CopyOperation::CopyOperation(
    DriveScheduler* drive_scheduler,
    DriveFileSystemInterface* drive_file_system,
    DriveResourceMetadata* metadata,
    google_apis::DriveUploaderInterface* uploader,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OperationObserver* observer)
  : drive_scheduler_(drive_scheduler),
    drive_file_system_(drive_file_system),
    metadata_(metadata),
    uploader_(uploader),
    blocking_task_runner_(blocking_task_runner),
    observer_(observer),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CopyOperation::~CopyOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CopyOperation::Copy(const FilePath& src_file_path,
                         const FilePath& dest_file_path,
                         const FileOperationCallback& callback) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  DCHECK(!callback.is_null());

  metadata_->GetEntryInfoPairByPaths(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&CopyOperation::CopyAfterGetEntryInfoPair,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void CopyOperation::TransferFileFromRemoteToLocal(
    const FilePath& remote_src_file_path,
    const FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_file_system_->GetFileByPath(
      remote_src_file_path,
      base::Bind(&CopyOperation::OnGetFileCompleteForTransferFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_dest_file_path,
                 callback),
      google_apis::GetContentCallback());
}

void CopyOperation::OnGetFileCompleteForTransferFile(
    const FilePath& local_dest_file_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  // GetFileByPath downloads the file from Drive to a local cache, which is then
  // copied to the actual destination path on the local file system using
  // CopyLocalFileOnBlockingPool.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&CopyLocalFileOnBlockingPool,
                 local_file_path,
                 local_dest_file_path),
      callback);
}

void CopyOperation::TransferFileFromLocalToRemote(
    const FilePath& local_src_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  metadata_->GetEntryInfoByPath(
      remote_dest_file_path.DirName(),
      base::Bind(
          &CopyOperation::TransferFileFromLocalToRemoteAfterGetEntryInfo,
          weak_ptr_factory_.GetWeakPtr(),
          local_src_file_path,
          remote_dest_file_path,
          callback));
}

void CopyOperation::TransferRegularFile(
    const FilePath& local_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* content_type = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&net::GetMimeTypeFromFile,
                 remote_dest_file_path,
                 base::Unretained(content_type)),
      base::Bind(&CopyOperation::StartFileUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 StartFileUploadParams(local_file_path,
                                       remote_dest_file_path,
                                       callback),
                 base::Owned(content_type)));
}

void CopyOperation::CopyHostedDocumentToDirectory(
    const FilePath& dir_path,
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_scheduler_->CopyHostedDocument(
      resource_id,
      FilePath(new_name).AsUTF8Unsafe(),
      base::Bind(&CopyOperation::OnCopyHostedDocumentCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 dir_path,
                 callback));
}

void CopyOperation::OnCopyHostedDocumentCompleted(
    const FilePath& dir_path,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(resource_entry);

  // |entry| was added in the root directory on the server, so we should
  // first add it to |root_| to mirror the state and then move it to the
  // destination directory by MoveEntryFromRootDirectory().
  metadata_->AddEntryToDirectory(
      FilePath(kDriveRootDirectory),
      resource_entry.Pass(),
      base::Bind(&CopyOperation::MoveEntryFromRootDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 dir_path,
                 callback));
}

// TODO(mtomasz): Share with the file_system::MoveOperation class.
void CopyOperation::MoveEntryFromRootDirectory(
    const FilePath& directory_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK_EQ(kDriveRootDirectory, file_path.DirName().value());

  // Return if there is an error or |dir_path| is the root directory.
  if (error != DRIVE_FILE_OK ||
      directory_path == FilePath(kDriveRootDirectory)) {
    callback.Run(error);
    return;
  }

  metadata_->GetEntryInfoPairByPaths(
      file_path,
      directory_path,
      base::Bind(
          &CopyOperation::MoveEntryFromRootDirectoryAfterGetEntryInfoPair,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

// TODO(mtomasz): Share with the file_system::MoveOperation class.
void CopyOperation::MoveEntryFromRootDirectoryAfterGetEntryInfoPair(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != DRIVE_FILE_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != DRIVE_FILE_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<DriveEntryProto> src_proto = result->first.proto.Pass();
  scoped_ptr<DriveEntryProto> dir_proto = result->second.proto.Pass();

  if (!dir_proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  const FilePath& file_path = result->first.path;
  const FilePath& dir_path = result->second.path;
  drive_scheduler_->AddResourceToDirectory(
      GURL(dir_proto->content_url()),
      GURL(src_proto->edit_url()),
      base::Bind(&CopyOperation::MoveEntryToDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 dir_path,
                 base::Bind(&CopyOperation::NotifyAndRunFileOperationCallback,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
}
void CopyOperation::MoveEntryToDirectory(
    const FilePath& file_path,
    const FilePath& directory_path,
    const FileMoveCallback& callback,
    GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  const DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error, FilePath());
    return;
  }

  metadata_->MoveEntryToDirectory(file_path, directory_path, callback);
}

void CopyOperation::NotifyAndRunFileOperationCallback(
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& moved_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == DRIVE_FILE_OK)
    observer_->OnDirectoryChangedByOperation(moved_file_path.DirName());

  callback.Run(error);
}

void CopyOperation::CopyAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != DRIVE_FILE_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != DRIVE_FILE_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<DriveEntryProto> src_file_proto = result->first.proto.Pass();
  scoped_ptr<DriveEntryProto> dest_parent_proto = result->second.proto.Pass();

  if (!dest_parent_proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  } else if (src_file_proto->file_info().is_directory()) {
    // TODO(kochi): Implement copy for directories. In the interim,
    // we handle recursive directory copy in the file manager.
    // crbug.com/141596
    callback.Run(DRIVE_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (src_file_proto->file_specific_info().is_hosted_document()) {
    CopyHostedDocumentToDirectory(
        dest_file_path.DirName(),
        src_file_proto->resource_id(),
        // Drop the document extension, which should not be in the title.
        dest_file_path.BaseName().RemoveExtension().value(),
        callback);
    return;
  }

  // TODO(kochi): Reimplement this once the server API supports
  // copying of regular files directly on the server side. crbug.com/138273
  const FilePath& src_file_path = result->first.path;
  drive_file_system_->GetFileByPath(
      src_file_path,
      base::Bind(&CopyOperation::OnGetFileCompleteForCopy,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback),
      google_apis::GetContentCallback());
}

void CopyOperation::OnGetFileCompleteForCopy(
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& local_file_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  // This callback is only triggered for a regular file via Copy().
  DCHECK_EQ(REGULAR_FILE, file_type);
  TransferRegularFile(local_file_path, remote_dest_file_path, callback);
}

void CopyOperation::StartFileUpload(const StartFileUploadParams& params,
                                    const std::string* content_type,
                                    bool got_content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  // Make sure the destination directory exists.
  metadata_->GetEntryInfoByPath(
      params.remote_file_path.DirName(),
      base::Bind(&CopyOperation::StartFileUploadAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 got_content_type ? *content_type : kMimeTypeOctetStream));
}

void CopyOperation::StartFileUploadAfterGetEntryInfo(
    const StartFileUploadParams& params,
    const std::string& content_type,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (entry_proto.get() && !entry_proto->file_info().is_directory())
    error = DRIVE_FILE_ERROR_NOT_A_DIRECTORY;

  if (error != DRIVE_FILE_OK) {
    params.callback.Run(error);
    return;
  }
  DCHECK(entry_proto.get());

  uploader_->UploadNewFile(GURL(entry_proto->upload_url()),
                           params.remote_file_path,
                           params.local_file_path,
                           params.remote_file_path.BaseName().value(),
                           content_type,
                           base::Bind(&CopyOperation::OnTransferCompleted,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      params.callback));
}

void CopyOperation::OnTransferCompleted(
    const FileOperationCallback& callback,
    google_apis::DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == google_apis::DRIVE_UPLOAD_OK && resource_entry.get()) {
    drive_file_system_->AddUploadedFile(drive_path.DirName(),
                                        resource_entry.Pass(),
                                        file_path,
                                        callback);
  } else {
    callback.Run(DriveUploadErrorToDriveFileError(error));
  }
}

void CopyOperation::TransferFileFromLocalToRemoteAfterGetEntryInfo(
    const FilePath& local_src_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry_proto.get());
  if (!entry_proto->file_info().is_directory()) {
    // The parent of |remote_dest_file_path| is not a directory.
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&GetDocumentResourceIdOnBlockingPool, local_src_file_path),
      base::Bind(&CopyOperation::TransferFileForResourceId,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_src_file_path,
                 remote_dest_file_path,
                 callback));
}

void CopyOperation::TransferFileForResourceId(
    const FilePath& local_file_path,
    const FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (resource_id.empty()) {
    // If |resource_id| is empty, upload the local file as a regular file.
    TransferRegularFile(local_file_path, remote_dest_file_path, callback);
    return;
  }

  // Otherwise, copy the document on the server side and add the new copy
  // to the destination directory (collection).
  CopyHostedDocumentToDirectory(
      remote_dest_file_path.DirName(),
      resource_id,
      // Drop the document extension, which should not be
      // in the document title.
      remote_dest_file_path.BaseName().RemoveExtension().value(),
      callback);
}

}  // namespace file_system
}  // namespace drive
