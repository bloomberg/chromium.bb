// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"

#include <string>

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

const char kMimeTypeOctetStream[] = "application/octet-stream";

// Wraps |callback| and always passes FILE_ERROR_OK when invoked.
// This is used for adopting |callback| to wait for a non-fatal operation.
void IgnoreError(const FileOperationCallback& callback, FileError error) {
  callback.Run(FILE_ERROR_OK);
}

}  // namespace

CreateFileOperation::CreateFileOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      cache_(cache),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CreateFileOperation::~CreateFileOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CreateFileOperation::CreateFile(const base::FilePath& file_path,
                                     bool is_exclusive,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // First, checks the existence of a file at |file_path|.
  metadata_->GetResourceEntryPairByPathsOnUIThread(
      file_path.DirName(),
      file_path,
      base::Bind(&CreateFileOperation::CreateFileAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 is_exclusive,
                 callback));
}

void CreateFileOperation::CreateFileAfterGetResourceEntry(
    const base::FilePath& file_path,
    bool is_exclusive,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> pair_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = pair_result->second.error;
  scoped_ptr<ResourceEntry> parent(pair_result->first.entry.Pass());

  // If parent path is not a directory, it is an error.
  if (!parent || !parent->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // If an entry already exists at |path|:
  if (error == FILE_ERROR_OK) {
    scoped_ptr<ResourceEntry> entry(pair_result->second.entry.Pass());
    // Error if an exclusive mode is requested, or the entry is not a file.
    if (is_exclusive ||
        entry->file_info().is_directory() ||
        entry->file_specific_info().is_hosted_document()) {
      callback.Run(FILE_ERROR_EXISTS);
    } else {
      callback.Run(FILE_ERROR_OK);
    }
    return;
  }

  // Otherwise, the error code must be ERROR_NOT_FOUND.
  if (error != FILE_ERROR_NOT_FOUND) {
    callback.Run(error);
    return;
  }

  // Get the mime type from the file name extension.
  std::string* content_type = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&net::GetMimeTypeFromFile,
                 file_path,
                 base::Unretained(content_type)),
      base::Bind(&CreateFileOperation::CreateFileAfterGetMimeType,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 parent->resource_id(),
                 callback,
                 base::Owned(content_type)));
}

void CreateFileOperation::CreateFileAfterGetMimeType(
    const base::FilePath& file_path,
    const std::string& parent_resource_id,
    const FileOperationCallback& callback,
    const std::string* content_type,
    bool got_content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scheduler_->CreateFile(
      parent_resource_id,
      file_path,
      file_path.BaseName().value(),
      got_content_type ? *content_type : kMimeTypeOctetStream,
      DriveClientContext(USER_INITIATED),
      base::Bind(&CreateFileOperation::CreateFileAfterUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void CreateFileOperation::CreateFileAfterUpload(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == google_apis::HTTP_SUCCESS && resource_entry) {
    // Add the entry to the local resource metadata.
    ResourceEntry entry = ConvertToResourceEntry(*resource_entry);
    metadata_->AddEntryOnUIThread(
        entry,
        base::Bind(&CreateFileOperation::CreateFileAfterAddToMetadata,
                   weak_ptr_factory_.GetWeakPtr(),
                   entry,
                   callback));
  } else {
    callback.Run(util::GDataToFileError(error));
  }
}

void CreateFileOperation::CreateFileAfterAddToMetadata(
    const ResourceEntry& entry,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& drive_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Depending on timing, the metadata may have inserted via change list feed
  // already. So, FILE_ERROR_EXISTS is not an error.
  if (error == FILE_ERROR_EXISTS)
    error = FILE_ERROR_OK;

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  observer_->OnDirectoryChangedByOperation(drive_path.DirName());

  // At this point, upload to the server is fully succeeded. Failure to store to
  // cache is not a fatal error, so we wrap the callback with IgnoreError, and
  // always return success to the caller.
  cache_->StoreOnUIThread(entry.resource_id(),
                          entry.file_specific_info().file_md5(),
                          base::FilePath(util::kSymLinkToDevNull),
                          internal::FileCache::FILE_OPERATION_COPY,
                          base::Bind(&IgnoreError, callback));
}

}  // namespace file_system
}  // namespace drive
