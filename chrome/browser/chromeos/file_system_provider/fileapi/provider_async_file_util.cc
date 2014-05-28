// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace internal {
namespace {

// Executes GetFileInfo on the UI thread.
void GetFileInfoOnUIThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION, base::File::Info());
    return;
  }

  parser.file_system()->GetMetadata(parser.file_path(), callback);
}

// Routes the response of GetFileInfo back to the IO thread.
void OnGetFileInfo(const fileapi::AsyncFileUtil::GetFileInfoCallback& callback,
                   base::File::Error result,
                   const base::File::Info& file_info) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result, file_info));
}

// Executes ReadDirectory on the UI thread.
void ReadDirectoryOnUIThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
                 fileapi::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return;
  }

  parser.file_system()->ReadDirectory(parser.file_path(), callback);
}

// Routes the response of ReadDirectory back to the IO thread.
void OnReadDirectory(
    const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback,
    base::File::Error result,
    const fileapi::AsyncFileUtil::EntryList& entry_list,
    bool has_more) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, result, entry_list, has_more));
}

}  // namespace

ProviderAsyncFileUtil::ProviderAsyncFileUtil() {}

ProviderAsyncFileUtil::~ProviderAsyncFileUtil() {}

void ProviderAsyncFileUtil::CreateOrOpen(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if ((file_flags & base::File::FLAG_CREATE) ||
      (file_flags & base::File::FLAG_OPEN_ALWAYS) ||
      (file_flags & base::File::FLAG_CREATE_ALWAYS) ||
      (file_flags & base::File::FLAG_OPEN_TRUNCATED)) {
    callback.Run(base::File(base::File::FILE_ERROR_ACCESS_DENIED),
                 base::Closure());
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
               base::Closure());
}

void ProviderAsyncFileUtil::EnsureFileExists(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED, false /* created */);
}

void ProviderAsyncFileUtil::CreateDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::GetFileInfo(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GetFileInfoOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     base::Bind(&OnGetFileInfo, callback)));
}

void ProviderAsyncFileUtil::ReadDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ReadDirectoryOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     base::Bind(&OnReadDirectory, callback)));
}

void ProviderAsyncFileUtil::Touch(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::Truncate(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::CopyFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::MoveFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::CopyInForeignFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::DeleteFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::DeleteDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::DeleteRecursively(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
               base::File::Info(),
               base::FilePath(),
               scoped_refptr<webkit_blob::ShareableFileReference>());
}

}  // namespace internal
}  // namespace file_system_provider
}  // namespace chromeos
