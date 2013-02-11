// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

struct CreateDirectoryOperation::CreateDirectoryParams {
  CreateDirectoryParams(const base::FilePath& target_directory_path,
                        bool is_exclusive,
                        bool is_recursive,
                        const FileOperationCallback& callback)
      : target_directory_path(target_directory_path),
        is_exclusive(is_exclusive),
        is_recursive(is_recursive),
        callback(callback) {}
  ~CreateDirectoryParams() {}

  const base::FilePath target_directory_path;
  const bool is_exclusive;
  const bool is_recursive;
  FileOperationCallback callback;
};

// CreateDirectoryOperation::FindFirstMissingParentDirectoryResult
// implementation.
CreateDirectoryOperation::FindFirstMissingParentDirectoryResult::
FindFirstMissingParentDirectoryResult()
    : error(CreateDirectoryOperation::FIND_FIRST_FOUND_INVALID) {
}

void CreateDirectoryOperation::FindFirstMissingParentDirectoryResult::Init(
    FindFirstMissingParentDirectoryError in_error,
    base::FilePath in_first_missing_parent_path,
    const std::string& in_last_dir_resource_id) {
  error = in_error;
  first_missing_parent_path = in_first_missing_parent_path;
  last_dir_resource_id = in_last_dir_resource_id;
}

CreateDirectoryOperation::FindFirstMissingParentDirectoryResult::
~FindFirstMissingParentDirectoryResult() {
}

struct CreateDirectoryOperation::FindFirstMissingParentDirectoryParams {
  FindFirstMissingParentDirectoryParams(
      const std::vector<base::FilePath::StringType>& path_parts,
      const FindFirstMissingParentDirectoryCallback& callback)
      : path_parts(path_parts),
        index(0),
        callback(callback) {
    DCHECK(!callback.is_null());
  }
  ~FindFirstMissingParentDirectoryParams() {}

  std::vector<base::FilePath::StringType> path_parts;
  size_t index;
  base::FilePath current_path;
  std::string last_dir_resource_id;
  const FindFirstMissingParentDirectoryCallback callback;
};

CreateDirectoryOperation::CreateDirectoryOperation(
    DriveScheduler* drive_scheduler,
    DriveResourceMetadata* metadata,
    OperationObserver* observer)
    : drive_scheduler_(drive_scheduler),
      metadata_(metadata),
      observer_(observer),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

  scoped_ptr<CreateDirectoryParams> params(new CreateDirectoryParams(
      directory_path, is_exclusive, is_recursive, callback));

  FindFirstMissingParentDirectory(
      directory_path,
      base::Bind(
          &CreateDirectoryOperation::CreateDirectoryAfterFindFirstMissingPath,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&params)));
}

void CreateDirectoryOperation::CreateDirectoryAfterFindFirstMissingPath(
    scoped_ptr<CreateDirectoryParams> params,
    const FindFirstMissingParentDirectoryResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());

  switch (result.error) {
    case FIND_FIRST_FOUND_INVALID: {
      params->callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
      return;
    }
    case FIND_FIRST_DIRECTORY_ALREADY_PRESENT: {
      params->callback.Run(
          params->is_exclusive ? DRIVE_FILE_ERROR_EXISTS : DRIVE_FILE_OK);
      return;
    }
    case FIND_FIRST_FOUND_MISSING: {
      // There is a missing folder to be created here, move on with the rest of
      // this function.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  // Do we have a parent directory here as well? We can't then create target
  // directory if this is not a recursive operation.
  if (params->target_directory_path != result.first_missing_parent_path &&
      !params->is_recursive) {
    params->callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  drive_scheduler_->AddNewDirectory(
      result.last_dir_resource_id,
      result.first_missing_parent_path.BaseName().AsUTF8Unsafe(),
      base::Bind(&CreateDirectoryOperation::AddNewDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 result.first_missing_parent_path));
}

void CreateDirectoryOperation::AddNewDirectory(
    scoped_ptr<CreateDirectoryParams> params,
    const base::FilePath& created_directory_path,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    params->callback.Run(error);
    return;
  }

  metadata_->AddEntryToDirectory(
      created_directory_path.DirName(),
      entry.Pass(),
      base::Bind(&CreateDirectoryOperation::ContinueCreateDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 created_directory_path));
}

void CreateDirectoryOperation::ContinueCreateDirectory(
    scoped_ptr<CreateDirectoryParams> params,
    const base::FilePath& created_directory_path,
    DriveFileError error,
    const base::FilePath& moved_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());

  if (error != DRIVE_FILE_OK) {
    params->callback.Run(error);
    return;
  }

  observer_->OnDirectoryChangedByOperation(moved_file_path.DirName());

  // Not done yet with recursive directory creation?
  if (params->target_directory_path != created_directory_path &&
      params->is_recursive) {
    CreateDirectory(
        params->target_directory_path, params->is_exclusive,
        params->is_recursive, params->callback);
  } else {
    // Finally done with the create request.
    params->callback.Run(DRIVE_FILE_OK);
  }
}

void CreateDirectoryOperation::FindFirstMissingParentDirectory(
    const base::FilePath& directory_path,
    const FindFirstMissingParentDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::vector<base::FilePath::StringType> path_parts;
  directory_path.GetComponents(&path_parts);

  scoped_ptr<FindFirstMissingParentDirectoryParams> params(
      new FindFirstMissingParentDirectoryParams(path_parts, callback));

  // Have to post because FindFirstMissingParentDirectoryInternal calls
  // the callback directly.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &CreateDirectoryOperation::FindFirstMissingParentDirectoryInternal,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&params)));
}

void CreateDirectoryOperation::FindFirstMissingParentDirectoryInternal(
    scoped_ptr<FindFirstMissingParentDirectoryParams> params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params.get());

  // Terminate recursion if we're at the last element.
  if (params->index == params->path_parts.size()) {
    FindFirstMissingParentDirectoryResult result;
    result.Init(FIND_FIRST_DIRECTORY_ALREADY_PRESENT, base::FilePath(), "");
    params->callback.Run(result);
    return;
  }

  params->current_path = params->current_path.Append(
      params->path_parts[params->index]);
  // Need a reference to current_path before we call base::Passed because the
  // order of evaluation of arguments is indeterminate.
  const base::FilePath& current_path = params->current_path;
  metadata_->GetEntryInfoByPath(
      current_path,
      base::Bind(
          &CreateDirectoryOperation::ContinueFindFirstMissingParentDirectory,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&params)));
}

void CreateDirectoryOperation::ContinueFindFirstMissingParentDirectory(
    scoped_ptr<FindFirstMissingParentDirectoryParams> params,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params.get());

  FindFirstMissingParentDirectoryResult result;
  if (error == DRIVE_FILE_ERROR_NOT_FOUND) {
    // Found the missing parent.
    result.Init(FIND_FIRST_FOUND_MISSING,
                params->current_path,
                params->last_dir_resource_id);
    params->callback.Run(result);
  } else if (error != DRIVE_FILE_OK ||
             !entry_proto->file_info().is_directory()) {
    // Unexpected error, or found a file when we were expecting a directory.
    result.Init(FIND_FIRST_FOUND_INVALID, base::FilePath(), "");
    params->callback.Run(result);
  } else {
    // This parent exists, so recursively look at the next element.
    params->last_dir_resource_id = entry_proto->resource_id();
    params->index++;
    FindFirstMissingParentDirectoryInternal(params.Pass());
  }
}

}  // namespace file_system
}  // namespace drive
