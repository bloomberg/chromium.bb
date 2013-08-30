// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

// Part of CreateDirectoryRecursively(). Adds an |entry| for new directory
// to |metadata|, and return the status. If succeeded, |file_path| will store
// the path to the result file.
FileError UpdateLocalStateForCreateDirectoryRecursively(
    internal::ResourceMetadata* metadata,
    scoped_ptr<google_apis::ResourceEntry> resource_entry,
    base::FilePath* file_path) {
  DCHECK(metadata);
  DCHECK(file_path);

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_NOT_A_FILE;

  std::string parent_local_id;
  FileError result = metadata->GetIdByResourceId(parent_resource_id,
                                                 &parent_local_id);
  if (result != FILE_ERROR_OK)
    return result;
  entry.set_parent_local_id(parent_local_id);

  std::string local_id;
  result = metadata->AddEntry(entry, &local_id);
  // Depending on timing, a metadata may be updated by change list already.
  // So, FILE_ERROR_EXISTS is not an error.
  if (result == FILE_ERROR_EXISTS)
    result = metadata->GetIdByResourceId(entry.resource_id(), &local_id);

  if (result == FILE_ERROR_OK)
    *file_path = metadata->GetFilePath(local_id);

  return result;
}

}  // namespace

CreateDirectoryOperation::CreateDirectoryOperation(
    base::SequencedTaskRunner* blocking_task_runner,
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

CreateDirectoryOperation::~CreateDirectoryOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CreateDirectoryOperation::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CreateDirectoryOperation::GetExistingDeepestDirectory,
                 metadata_,
                 directory_path,
                 entry),
      base::Bind(&CreateDirectoryOperation::
                     CreateDirectoryAfterGetExistingDeepestDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 is_exclusive,
                 is_recursive,
                 callback,
                 base::Owned(entry)));
}

// static
base::FilePath CreateDirectoryOperation::GetExistingDeepestDirectory(
    internal::ResourceMetadata* metadata,
    const base::FilePath& directory_path,
    ResourceEntry* entry) {
  DCHECK(metadata);
  DCHECK(entry);

  std::vector<base::FilePath::StringType> components;
  directory_path.GetComponents(&components);

  if (components.empty() || components[0] != util::kDriveGrandRootDirName)
    return base::FilePath();

  std::string local_id = util::kDriveGrandRootSpecialResourceId;
  for (size_t i = 1; i < components.size(); ++i) {
    std::string child_local_id = metadata->GetChildId(local_id, components[i]);
    if (child_local_id.empty())
      break;
    local_id = child_local_id;
  }

  FileError error = metadata->GetResourceEntryById(local_id, entry);
  DCHECK_EQ(FILE_ERROR_OK, error);

  if (!entry->file_info().is_directory())
    return base::FilePath();

  return metadata->GetFilePath(local_id);
}

void CreateDirectoryOperation::CreateDirectoryAfterGetExistingDeepestDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback,
    ResourceEntry* existing_deepest_directory_entry,
    const base::FilePath& existing_deepest_directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(existing_deepest_directory_entry);

  if (existing_deepest_directory_path.empty()) {
    callback.Run(FILE_ERROR_NOT_FOUND);
    return;
  }

  if (directory_path == existing_deepest_directory_path) {
    callback.Run(is_exclusive ? FILE_ERROR_EXISTS : FILE_ERROR_OK);
    return;
  }

  // If it is not recursive creation, the found directory must be the direct
  // parent of |directory_path| to ensure creating exact one directory.
  if (!is_recursive &&
      existing_deepest_directory_path != directory_path.DirName()) {
    callback.Run(FILE_ERROR_NOT_FOUND);
    return;
  }

  // Create directories under the found directory.
  base::FilePath remaining_path;
  existing_deepest_directory_path.AppendRelativePath(
      directory_path, &remaining_path);
  CreateDirectoryRecursively(existing_deepest_directory_entry->resource_id(),
                             remaining_path, callback);
}

void CreateDirectoryOperation::CreateDirectoryRecursively(
    const std::string& parent_resource_id,
    const base::FilePath& relative_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Split the first component and remaining ones of |relative_file_path|.
  std::vector<base::FilePath::StringType> components;
  relative_file_path.GetComponents(&components);
  DCHECK(!components.empty());
  base::FilePath title(components[0]);
  base::FilePath remaining_path;
  title.AppendRelativePath(relative_file_path, &remaining_path);

  scheduler_->AddNewDirectory(
      parent_resource_id,
      title.AsUTF8Unsafe(),
      base::Bind(&CreateDirectoryOperation
                     ::CreateDirectoryRecursivelyAfterAddNewDirectory,
                 weak_ptr_factory_.GetWeakPtr(), remaining_path, callback));
}

void CreateDirectoryOperation::CreateDirectoryRecursivelyAfterAddNewDirectory(
    const base::FilePath& remaining_path,
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
  const std::string& resource_id = resource_entry->resource_id();

  // Note that the created directory may be renamed inside
  // ResourceMetadata::AddEntry due to name confliction.
  // What we actually need here is the new created path (not the path we try
  // to create).
  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalStateForCreateDirectoryRecursively,
                 metadata_,
                 base::Passed(&resource_entry),
                 file_path),
      base::Bind(&CreateDirectoryOperation::
                     CreateDirectoryRecursivelyAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id,
                 remaining_path,
                 callback,
                 base::Owned(file_path)));
}

void CreateDirectoryOperation::CreateDirectoryRecursivelyAfterUpdateLocalState(
    const std::string& resource_id,
    const base::FilePath& remaining_path,
    const FileOperationCallback& callback,
    base::FilePath* file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  observer_->OnDirectoryChangedByOperation(file_path->DirName());

  if (remaining_path.empty()) {
    // All directories are created successfully.
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // Create descendant directories.
  CreateDirectoryRecursively(resource_id, remaining_path, callback);
}

}  // namespace file_system
}  // namespace drive
