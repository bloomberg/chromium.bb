// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Looks up ResourceEntry for source entry and the destination directory.
FileError PrepareMove(internal::ResourceMetadata* metadata,
                      const base::FilePath& src_path,
                      const base::FilePath& dest_parent_path,
                      ResourceEntry* src_entry,
                      ResourceEntry* src_parent_entry,
                      ResourceEntry* dest_parent_entry) {
  FileError error = metadata->GetResourceEntryByPath(src_path, src_entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryById(src_entry->parent_local_id(),
                                         src_parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  return metadata->GetResourceEntryByPath(dest_parent_path, dest_parent_entry);
}

// Refreshes the corresponding entry in the metadata with the given one.
FileError RefreshEntry(internal::ResourceMetadata* metadata,
                       scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_FAILED;

  std::string parent_local_id;
  FileError error = metadata->GetIdByResourceId(parent_resource_id,
                                                &parent_local_id);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_parent_local_id(parent_local_id);

  std::string local_id;
  error = metadata->GetIdByResourceId(entry.resource_id(), &local_id);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_local_id(local_id);

  return metadata->RefreshEntry(entry);
}

}  // namespace

struct MoveOperation::MoveParams {
  base::FilePath src_file_path;
  base::FilePath dest_file_path;
  bool preserve_last_modified;
  FileOperationCallback callback;
};

MoveOperation::MoveOperation(base::SequencedTaskRunner* blocking_task_runner,
                             OperationObserver* observer,
                             JobScheduler* scheduler,
                             internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

MoveOperation::~MoveOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MoveOperation::Move(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         bool preserve_last_modified,
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  MoveParams params;
  params.src_file_path = src_file_path;
  params.dest_file_path = dest_file_path;
  params.preserve_last_modified = preserve_last_modified;
  params.callback = callback;

  scoped_ptr<ResourceEntry> src_entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> src_parent_entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> dest_parent_entry(new ResourceEntry);
  ResourceEntry* src_entry_ptr = src_entry.get();
  ResourceEntry* src_parent_entry_ptr = src_parent_entry.get();
  ResourceEntry* dest_parent_entry_ptr = dest_parent_entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareMove,
                 metadata_, src_file_path, dest_file_path.DirName(),
                 src_entry_ptr, src_parent_entry_ptr, dest_parent_entry_ptr),
      base::Bind(&MoveOperation::MoveAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), params,
                 base::Passed(&src_entry),
                 base::Passed(&src_parent_entry),
                 base::Passed(&dest_parent_entry)));
}

void MoveOperation::MoveAfterPrepare(
    const MoveParams& params,
    scoped_ptr<ResourceEntry> src_entry,
    scoped_ptr<ResourceEntry> src_parent_entry,
    scoped_ptr<ResourceEntry> dest_parent_entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  if (!dest_parent_entry->file_info().is_directory()) {
    // The parent of the destination is not a directory.
    params.callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // Strip the extension for a hosted document if necessary.
  const bool has_hosted_document_extension =
      src_entry->has_file_specific_info() &&
      src_entry->file_specific_info().is_hosted_document() &&
      params.dest_file_path.Extension() ==
      src_entry->file_specific_info().document_extension();
  const std::string new_title =
      has_hosted_document_extension ?
      params.dest_file_path.BaseName().RemoveExtension().AsUTF8Unsafe() :
      params.dest_file_path.BaseName().AsUTF8Unsafe();

  base::Time last_modified =
      params.preserve_last_modified ?
      base::Time::FromInternalValue(src_entry->file_info().last_modified()) :
      base::Time();

  scheduler_->UpdateResource(
      src_entry->resource_id(), dest_parent_entry->resource_id(),
      new_title, last_modified, base::Time(),
      base::Bind(&MoveOperation::MoveAfterUpdateResource,
                 weak_ptr_factory_.GetWeakPtr(), params));
}

void MoveOperation::MoveAfterUpdateResource(
    const MoveParams& params,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&RefreshEntry, metadata_, base::Passed(&resource_entry)),
      base::Bind(&MoveOperation::MoveAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(), params));
}

void MoveOperation::MoveAfterRefreshEntry(const MoveParams& params,
                                          FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error == FILE_ERROR_OK) {
    // Notify the change of directory.
    observer_->OnDirectoryChangedByOperation(params.src_file_path.DirName());
    observer_->OnDirectoryChangedByOperation(params.dest_file_path.DirName());
  }

  params.callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
