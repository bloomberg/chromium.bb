// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace internal {

ProviderAsyncFileUtil::ProviderAsyncFileUtil() {}

ProviderAsyncFileUtil::~ProviderAsyncFileUtil() {}

void ProviderAsyncFileUtil::CreateOrOpen(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFile platform_file = base::kInvalidPlatformFileValue;
  if ((file_flags & base::PLATFORM_FILE_CREATE) ||
      (file_flags & base::PLATFORM_FILE_OPEN_ALWAYS) ||
      (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS) ||
      (file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED)) {
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 base::PassPlatformFile(&platform_file),
                 base::Closure());
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_NOT_FOUND,
               base::PassPlatformFile(&platform_file),
               base::Closure());
}

void ProviderAsyncFileUtil::EnsureFileExists(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY, false /* created */);
}

void ProviderAsyncFileUtil::CreateDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::GetFileInfo(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_NOT_FOUND, base::File::Info());
}

void ProviderAsyncFileUtil::ReadDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_NOT_FOUND, EntryList(), false);
}

void ProviderAsyncFileUtil::Touch(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::Truncate(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::CopyFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::MoveFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::CopyInForeignFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::DeleteFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::DeleteDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::DeleteRecursively(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProviderAsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_NOT_FOUND,
               base::File::Info(),
               base::FilePath(),
               scoped_refptr<webkit_blob::ShareableFileReference>());
}

}  // namespace internal
}  // namespace file_system_provider
}  // namespace chromeos
