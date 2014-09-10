// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/async_file_util.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi/fileapi_worker.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/blob/shareable_file_reference.h"

using content::BrowserThread;

namespace google_apis {
namespace internal {

// Partial specialization of helper template from google_apis/drive/task_util.h
// to enable google_apis::CreateRelayCallback to work with CreateOrOpenCallback.
template<typename T2>
struct ComposedCallback<void(base::File, T2)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(base::File, T2)>& callback,
      base::File arg1, T2 arg2) {
    runner.Run(base::Bind(callback, Passed(&arg1), arg2));
  }
};

}  // namespace internal
}  // namespace google_apis

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
                 base::Bind(&google_apis::RunTaskWithTaskRunner,
                            base::MessageLoopProxy::current(),
                            on_error_callback)));
}

// Runs CreateOrOpenFile callback based on the given |error| and |file|.
void RunCreateOrOpenFileCallback(
    const AsyncFileUtil::CreateOrOpenCallback& callback,
    base::File file,
    const base::Closure& close_callback_on_ui_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // It is necessary to make a closure, which runs on file closing here.
  // It will be provided as a FileSystem::OpenFileCallback's argument later.
  // (crbug.com/259184).
  callback.Run(
      file.Pass(),
      base::Bind(&google_apis::RunTaskWithTaskRunner,
                 BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
                 close_callback_on_ui_thread));
}

// Runs CreateOrOpenFile when the error happens.
void RunCreateOrOpenFileCallbackOnError(
    const AsyncFileUtil::CreateOrOpenCallback& callback,
    base::File::Error error) {
  callback.Run(base::File(error), base::Closure());
}

// Runs EnsureFileExistsCallback based on the given |error|.
void RunEnsureFileExistsCallback(
    const AsyncFileUtil::EnsureFileExistsCallback& callback,
    base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Remember if the file is actually created or not.
  bool created = (error == base::File::FILE_OK);

  // File::FILE_ERROR_EXISTS is not an actual error here.
  if (error == base::File::FILE_ERROR_EXISTS)
    error = base::File::FILE_OK;

  callback.Run(error, created);
}

// Runs |callback| with the arguments based on the given arguments.
void RunCreateSnapshotFileCallback(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::File::Error error,
    const base::File::Info& file_info,
    const base::FilePath& local_path,
    storage::ScopedFile::ScopeOutPolicy scope_out_policy) {
  // ShareableFileReference is thread *unsafe* class. So it is necessary to
  // create the instance (by invoking GetOrCreate) on IO thread, though
  // most drive file system related operations run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<storage::ShareableFileReference> file_reference =
      storage::ShareableFileReference::GetOrCreate(storage::ScopedFile(
          local_path, scope_out_policy, BrowserThread::GetBlockingPool()));
  callback.Run(error, file_info, local_path, file_reference);
}

}  // namespace

AsyncFileUtil::AsyncFileUtil() {
}

AsyncFileUtil::~AsyncFileUtil() {
}

void AsyncFileUtil::CreateOrOpen(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File(base::File::FILE_ERROR_NOT_FOUND), base::Closure());
    return;
  }

  const fileapi_internal::FileSystemGetter getter =
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url);
  PostFileSystemCallback(
      getter,
      base::Bind(&fileapi_internal::OpenFile,
                 file_path, file_flags,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunCreateOrOpenFileCallback, callback))),
      base::Bind(&RunCreateOrOpenFileCallbackOnError,
                 callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::EnsureFileExists(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, false);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::CreateFile,
                 file_path, true /* is_exlusive */,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunEnsureFileExistsCallback, callback))),
      base::Bind(callback, base::File::FILE_ERROR_FAILED, false));
}

void AsyncFileUtil::CreateDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::CreateDirectory,
                 file_path, exclusive, recursive,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::GetFileInfo(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, base::File::Info());
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::GetFileInfo,
                 file_path, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED,
                 base::File::Info()));
}

void AsyncFileUtil::ReadDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, EntryList(), false);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::ReadDirectory,
                 file_path, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED,
                 EntryList(), false));
}

void AsyncFileUtil::Touch(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::TouchFile,
                 file_path, last_access_time, last_modified_time,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::Truncate(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::Truncate,
                 file_path, length, google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::CopyFileLocal(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_path = util::ExtractDrivePathFromFileSystemUrl(src_url);
  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (src_path.empty() || dest_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // TODO(kinaba): crbug.com/339794.
  // Assumption here is that |src_url| and |dest_url| are always from the same
  // profile. This indeed holds as long as we mount different profiles onto
  // different mount point. Hence, using GetFileSystemFromUrl(dest_url) is safe.
  // This will change after we introduce cross-profile sharing etc., and we
  // need to deal with files from different profiles here.
  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, dest_url),
      base::Bind(
          &fileapi_internal::Copy,
          src_path,
          dest_path,
          option == storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
          google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::MoveFileLocal(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_path = util::ExtractDrivePathFromFileSystemUrl(src_url);
  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (src_path.empty() || dest_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // TODO(kinaba): see the comment in CopyFileLocal(). |src_url| and |dest_url|
  // always return the same FileSystem by GetFileSystemFromUrl, but we need to
  // change it in order to support cross-profile file sharing etc.
  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, dest_url),
      base::Bind(&fileapi_internal::Move,
                 src_path, dest_path,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::CopyInForeignFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath dest_path = util::ExtractDrivePathFromFileSystemUrl(dest_url);
  if (dest_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, dest_url),
      base::Bind(&fileapi_internal::CopyInForeignFile,
                 src_file_path, dest_path,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::Remove,
                 file_path, false /* not recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::Remove,
                 file_path, false /* not recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::DeleteRecursively(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::Remove,
                 file_path, true /* recursive */,
                 google_apis::CreateRelayCallback(callback)),
      base::Bind(callback, base::File::FILE_ERROR_FAILED));
}

void AsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND,
                 base::File::Info(),
                 base::FilePath(),
                 scoped_refptr<storage::ShareableFileReference>());
    return;
  }

  PostFileSystemCallback(
      base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
      base::Bind(&fileapi_internal::CreateSnapshotFile,
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunCreateSnapshotFileCallback, callback))),
      base::Bind(callback,
                 base::File::FILE_ERROR_FAILED,
                 base::File::Info(),
                 base::FilePath(),
                 scoped_refptr<storage::ShareableFileReference>()));
}

}  // namespace internal
}  // namespace drive
