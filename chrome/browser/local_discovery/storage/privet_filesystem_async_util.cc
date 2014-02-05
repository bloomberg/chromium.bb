// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_async_util.h"

#include "base/platform_file.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace local_discovery {

void PrivetFileSystemAsyncUtil::CreateOrOpen(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
               base::PassPlatformFile(NULL),
               base::Closure());
}

void PrivetFileSystemAsyncUtil::EnsureFileExists(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
    NOTIMPLEMENTED();
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION, false);
}

void PrivetFileSystemAsyncUtil::CreateDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
    NOTIMPLEMENTED();
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::GetFileInfo(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  base::File::Info file_info;

  file_info.size = 20;
  file_info.is_directory = true;
  file_info.is_symbolic_link = false;

  callback.Run(base::File::FILE_OK, file_info);
}

void PrivetFileSystemAsyncUtil::ReadDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  EntryList entry_list;

  fileapi::DirectoryEntry entry("Random file",
                                fileapi::DirectoryEntry::FILE,
                                3000,
                                base::Time());
  entry_list.push_back(entry);

  callback.Run(base::File::FILE_OK, entry_list, false);
}

void PrivetFileSystemAsyncUtil::Touch(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::Truncate(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::CopyFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::MoveFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::CopyInForeignFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::DeleteFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::DeleteDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::DeleteRecursively(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void PrivetFileSystemAsyncUtil::CreateSnapshotFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
               base::File::Info(),
               base::FilePath(),
               scoped_refptr<webkit_blob::ShareableFileReference>());
}

}  // namespace local_discovery
