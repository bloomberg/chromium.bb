// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_async_file_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

class Profile;

namespace chromeos {

RecentAsyncFileUtil::RecentAsyncFileUtil() : weak_ptr_factory_(this) {}

RecentAsyncFileUtil::~RecentAsyncFileUtil() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void RecentAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(nya): Implement this function if it is ever called.
  NOTIMPLEMENTED();
  callback.Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
               base::Closure());
}

void RecentAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED, false);
}

void RecentAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int fields,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(nya): Implement this function.
  callback.Run(base::File::FILE_ERROR_FAILED, base::File::Info());
}

void RecentAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(nya): Implement this function.
  callback.Run(base::File::FILE_ERROR_FAILED, EntryList(), false);
}

void RecentAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void RecentAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(crbug.com/671511): Implement this function.
  callback.Run(base::File::FILE_ERROR_FAILED, base::File::Info(),
               base::FilePath(),
               scoped_refptr<storage::ShareableFileReference>());
}

}  // namespace chromeos
