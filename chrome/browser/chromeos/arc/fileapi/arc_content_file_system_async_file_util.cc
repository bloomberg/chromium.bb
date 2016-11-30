// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_async_file_util.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_instance_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace arc {

namespace {

void OnGetFileSize(const storage::AsyncFileUtil::GetFileInfoCallback& callback,
                   int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::File::Info info;
  base::File::Error error = base::File::FILE_OK;
  if (size == -1) {
    error = base::File::FILE_ERROR_FAILED;
  } else {
    info.size = size;
  }
  callback.Run(error, info);
}

}  // namespace

ArcContentFileSystemAsyncFileUtil::ArcContentFileSystemAsyncFileUtil() =
    default;

ArcContentFileSystemAsyncFileUtil::~ArcContentFileSystemAsyncFileUtil() =
    default;

void ArcContentFileSystemAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::Passed(base::File()), base::Closure()));
}

void ArcContentFileSystemAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED, false));
}

void ArcContentFileSystemAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int fields,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_instance_util::GetFileSizeOnIOThread(
      FileSystemUrlToArcUrl(url), base::Bind(&OnGetFileSize, callback));
}

void ArcContentFileSystemAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::File::FILE_ERROR_FAILED, EntryList(), false));
}

void ArcContentFileSystemAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void ArcContentFileSystemAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  NOTIMPLEMENTED();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_FAILED,
                            base::File::Info(), base::FilePath(),
                            scoped_refptr<storage::ShareableFileReference>()));
}

}  // namespace arc
