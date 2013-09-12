// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/async_file_util.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi_worker.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Posts fileapi_internal::RunFileSystemCallback to UI thread.
// This function must be called on IO thread.
// The |on_error_callback| will be called (on error case) on IO thread.
void PostFileSystemCallback(
    const fileapi_internal::FileSystemGetter& file_system_getter,
    const base::Callback<void(FileSystemInterface*)>& function,
    const base::Closure& on_error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&fileapi_internal::RunFileSystemCallback,
                 file_system_getter, function,
                 on_error_callback.is_null() ?
                 base::Closure() :
                 base::Bind(&google_apis::RunTaskOnThread,
                            base::MessageLoopProxy::current(),
                            on_error_callback)));
}

// Runs CreateOrOpenFile callback based on the given |error| and |file|.
void RunCreateOrOpenFileCallback(
    const AsyncFileUtil::FileSystemGetter& file_system_getter,
    const base::FilePath& file_path,
    const AsyncFileUtil::CreateOrOpenCallback& callback,
    base::PlatformFileError error,
    base::PlatformFile file,
    const base::Closure& close_callback_on_ui_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // It is necessary to make a closure, which runs on file closing here.
  // It will be provided as a FileSystem::OpenFileCallback's argument later.
  // (crbug.com/259184).
  callback.Run(
      error, base::PassPlatformFile(&file),
      base::Bind(&google_apis::RunTaskOnThread,
                 BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
                 close_callback_on_ui_thread));
}

// Runs CreateOrOpenFile when the error happens.
void RunCreateOrOpenFileCallbackOnError(
    const AsyncFileUtil::CreateOrOpenCallback& callback,
    base::PlatformFileError error) {
  // Because the |callback| takes PassPlatformFile as its argument, and
  // it is necessary to guarantee the pointer passed to PassPlatformFile is
  // alive during the |callback| invocation, here we prepare a thin adapter
  // to have PlatformFile on stack frame.
  base::PlatformFile file = base::kInvalidPlatformFileValue;
  callback.Run(error, base::PassPlatformFile(&file), base::Closure());
}

// Runs EnsureFileExistsCallback based on the given |error|.
void RunEnsureFileExistsCallback(
    const AsyncFileUtil::EnsureFileExistsCallback& callback,
    base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Remember if the file is actually created or not.
  bool created = (error == base::PLATFORM_FILE_OK);

  // PLATFORM_FILE_ERROR_EXISTS is not an actual error here.
  if (error == base::PLATFORM_FILE_ERROR_EXISTS)
    error = base::PLATFORM_FILE_OK;

  callback.Run(error, created);
}


// Runs |callback| with the arguments based on the given arguments.
void RunCreateSnapshotFileCallback(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& local_path,
    webkit_blob::ScopedFile::ScopeOutPolicy scope_out_policy) {
  // ShareableFileReference is thread *unsafe* class. So it is necessary to
  // create the instance (by invoking GetOrCreate) on IO thread, though
  // most drive file system related operations run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<webkit_blob::ShareableFileReference> file_reference =
      webkit_blob::ShareableFileReference::GetOrCreate(webkit_blob::ScopedFile(
          local_path,
          scope_out_policy,
          BrowserThread::GetBlockingPool()));
  callback.Run(error, file_info, local_path, file_reference);
}

}  // namespace

AsyncFileUtil::AsyncFileUtil(const FileSystemGetter& file_system_getter)
    : file_system_getter_(file_system_getter) {
}

AsyncFileUtil::~AsyncFileUtil() {
}

void AsyncFileUtil::CreateOrOpen(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    base::PlatformFile platform_file = base::kInvalidPlatformFileValue;
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PassPlatformFile(&platform_file),
                 base::Closure());
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::OpenFile,
                 file_path, file_flags,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunCreateOrOpenFileCallback,
                                file_system_getter_, file_path, callback))),
      base::Bind(&RunCreateOrOpenFileCallbackOnError,
                 callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::EnsureFileExists(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, false);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::CreateFile,
                 file_path, true /* is_exlusive */,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunEnsureFileExistsCallback, callback))),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED, false));
}

void AsyncFileUtil::CreateDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::CreateDirectory,
                 file_path, exclusive, recursive,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::GetFileInfo(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, base::PlatformFileInfo());
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::GetFileInfo,
                 file_path, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED,
                 base::PlatformFileInfo()));
}

void AsyncFileUtil::ReadDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, EntryList(), false);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::ReadDirectory,
                 file_path, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED,
                 EntryList(), false));
}

void AsyncFileUtil::Touch(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::TouchFile,
                 file_path, last_access_time, last_modified_time,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::Truncate(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Truncate,
                 file_path, length, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::CopyFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_path = util::ExtractDrivePathFromFileSystemUrl(src_url);
  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (src_path.empty() || dest_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Copy,
                 src_path, dest_path,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::MoveFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_path = util::ExtractDrivePathFromFileSystemUrl(src_url);
  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (src_path.empty() || dest_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Move,
                 src_path, dest_path,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::CopyInForeignFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (dest_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::CopyInForeignFile,
                 src_file_path, dest_path,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Remove,
                 file_path, false /* not recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Remove,
                 file_path, false /* not recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteRecursively(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::Remove,
                 file_path, true /* recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void AsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PlatformFileInfo(),
                 base::FilePath(),
                 scoped_refptr<webkit_blob::ShareableFileReference>());
    return;
  }

  PostFileSystemCallback(
      file_system_getter_,
      base::Bind(&fileapi_internal::CreateSnapshotFile,
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunCreateSnapshotFileCallback, callback))),
      base::Bind(callback,
                 base::PLATFORM_FILE_ERROR_FAILED,
                 base::PlatformFileInfo(),
                 base::FilePath(),
                 scoped_refptr<webkit_blob::ShareableFileReference>()));
}

}  // namespace internal
}  // namespace drive
