// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"

#include <string>

#include "base/file_util.h"
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

// Part of CreateFileOperation::CreateFile(), runs on |blocking_task_runner_|
// of the operation, before server-side file creation.
FileError CheckPreConditionForCreateFile(internal::ResourceMetadata* metadata,
                                         const base::FilePath& file_path,
                                         bool is_exclusive,
                                         std::string* parent_resource_id,
                                         std::string* mime_type) {
  DCHECK(metadata);
  DCHECK(parent_resource_id);
  DCHECK(mime_type);

  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryByPath(file_path, &entry);
  if (error == FILE_ERROR_OK) {
    // Error if an exclusive mode is requested, or the entry is not a file.
    return (is_exclusive ||
            entry.file_info().is_directory() ||
            entry.file_specific_info().is_hosted_document()) ?
        FILE_ERROR_EXISTS : FILE_ERROR_OK;
  }

  // If the file is not found, an actual request to create a new file will be
  // sent to the server.
  if (error == FILE_ERROR_NOT_FOUND) {
    // If parent path is not a directory, it is an error.
    ResourceEntry parent;
    if (metadata->GetResourceEntryByPath(
            file_path.DirName(), &parent) != FILE_ERROR_OK ||
        !parent.file_info().is_directory())
      return FILE_ERROR_NOT_A_DIRECTORY;

    // In the request, parent_resource_id and mime_type are needed.
    // Here, populate them.
    *parent_resource_id = parent.resource_id();

    // If mime_type is not set or "application/octet-stream", guess from the
    // |file_path|. If it is still unsure, use octet-stream by default.
    if ((mime_type->empty() || *mime_type == kMimeTypeOctetStream) &&
        !net::GetMimeTypeFromFile(file_path, mime_type)) {
      *mime_type = kMimeTypeOctetStream;
    }
  }

  return error;
}

// Part of CreateFileOperation::CreateFile(), runs on |blocking_task_runner_|
// of the operation, after server side file creation.
FileError UpdateLocalStateForCreateFile(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    scoped_ptr<google_apis::ResourceEntry> resource_entry,
    base::FilePath* file_path) {
  DCHECK(metadata);
  DCHECK(cache);
  DCHECK(resource_entry);
  DCHECK(file_path);

  // Add the entry to the local resource metadata.
  FileError error = FILE_ERROR_NOT_A_FILE;
  ResourceEntry entry;
  if (ConvertToResourceEntry(*resource_entry, &entry))
    error = metadata->AddEntry(entry);

  // Depending on timing, the metadata may have inserted via change list
  // already. So, FILE_ERROR_EXISTS is not an error.
  if (error == FILE_ERROR_EXISTS)
    error = FILE_ERROR_OK;

  if (error == FILE_ERROR_OK) {
    // At this point, upload to the server is fully succeeded.
    // Populate the |file_path| which will be used to notify the observer.
    *file_path = metadata->GetFilePath(entry.resource_id());

    // Also store an empty file to the cache.
    // Here, failure is not a fatal error, so ignore the returned code.
    FileError cache_store_error = FILE_ERROR_FAILED;
    base::FilePath empty_file;
    if (file_util::CreateTemporaryFile(&empty_file)) {
      cache_store_error =  cache->Store(
          entry.resource_id(),
          entry.file_specific_info().md5(),
          empty_file,
          internal::FileCache::FILE_OPERATION_MOVE);
    }
    DLOG_IF(WARNING, cache_store_error != FILE_ERROR_OK)
        << "Failed to store a cache file: "
        << FileErrorToString(cache_store_error)
        << ", resource_id: " << entry.resource_id();
  }

  return error;
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
                                     const std::string& mime_type,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* parent_resource_id = new std::string;
  std::string* determined_mime_type = new std::string(mime_type);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckPreConditionForCreateFile,
                 metadata_,
                 file_path,
                 is_exclusive,
                 parent_resource_id,
                 determined_mime_type),
      base::Bind(&CreateFileOperation::CreateFileAfterCheckPreCondition,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback,
                 base::Owned(parent_resource_id),
                 base::Owned(determined_mime_type)));
}

void CreateFileOperation::CreateFileAfterCheckPreCondition(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    std::string* parent_resource_id,
    std::string* mime_type,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(parent_resource_id);
  DCHECK(mime_type);

  // If the file is found, or an error other than "not found" is found,
  // runs callback and quit the operation.
  if (error != FILE_ERROR_NOT_FOUND) {
    callback.Run(error);
    return;
  }

  scheduler_->CreateFile(
      *parent_resource_id,
      file_path,
      file_path.BaseName().value(),
      *mime_type,
      ClientContext(USER_INITIATED),
      base::Bind(&CreateFileOperation::CreateFileAfterUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void CreateFileOperation::CreateFileAfterUpload(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(resource_entry);

  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalStateForCreateFile,
                 metadata_,
                 cache_,
                 base::Passed(&resource_entry),
                 file_path),
      base::Bind(&CreateFileOperation::CreateFileAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(file_path)));
}

void CreateFileOperation::CreateFileAfterUpdateLocalState(
    const FileOperationCallback& callback,
    base::FilePath* file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(file_path);

  // Notify observer if the file creation process is successfully done.
  if (error == FILE_ERROR_OK)
    observer_->OnDirectoryChangedByOperation(file_path->DirName());

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
