// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"

#include <stddef.h>

#include <utility>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/fileapi/mtp_file_stream_reader.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/fileapi/readahead_file_stream_reader.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/native_file_util.h"

using storage::AsyncFileUtil;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::ShareableFileReference;

namespace {

const char kDeviceMediaAsyncFileUtilTempDir[] = "DeviceMediaFileSystem";

MTPDeviceAsyncDelegate* GetMTPDeviceDelegate(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(
      url.filesystem_id());
}

// Called when GetFileInfo method call failed to get the details of file
// specified by the requested url. |callback| is invoked to notify the
// caller about the file |error|.
void OnGetFileInfoError(const AsyncFileUtil::GetFileInfoCallback& callback,
                        base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error, base::File::Info());
}

// Called after OnDidGetFileInfo finishes media check.
// |callback| is invoked to complete the GetFileInfo request.
void OnDidCheckMediaForGetFileInfo(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::File::Info& file_info,
    bool is_valid_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!is_valid_file) {
    OnGetFileInfoError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  callback.Run(base::File::FILE_OK, file_info);
}

// Called after OnDidReadDirectory finishes media check.
// |callback| is invoked to complete the ReadDirectory request.
void OnDidCheckMediaForReadDirectory(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    bool has_more,
    AsyncFileUtil::EntryList file_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(base::File::FILE_OK, std::move(file_list), has_more);
}

// Called when CreateDirectory method call failed.
void OnCreateDirectoryError(const AsyncFileUtil::StatusCallback& callback,
                            base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called when ReadDirectory method call failed to enumerate the directory
// objects. |callback| is invoked to notify the caller about the |error|
// that occured while reading the directory objects.
void OnReadDirectoryError(const AsyncFileUtil::ReadDirectoryCallback& callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error, AsyncFileUtil::EntryList(), false /*no more*/);
}

// Called when CopyFileLocal method call failed.
void OnCopyFileLocalError(const AsyncFileUtil::StatusCallback& callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called when MoveFileLocal method call failed.
void OnMoveFileLocalError(const AsyncFileUtil::StatusCallback& callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called when CopyInForeignFile method call failed.
void OnCopyInForeignFileError(const AsyncFileUtil::StatusCallback& callback,
                              base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called when DeleteFile method call failed.
void OnDeleteFileError(const AsyncFileUtil::StatusCallback& callback,
                       base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called when DeleteDirectory method call failed.
void OnDeleteDirectoryError(const AsyncFileUtil::StatusCallback& callback,
                            base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error);
}

// Called on a blocking pool thread to create a snapshot file to hold the
// contents of |device_file_path|. The snapshot file is created in the
// "profile_path/kDeviceMediaAsyncFileUtilTempDir" directory. Return the
// snapshot file path or an empty path on failure.
base::FilePath CreateSnapshotFileOnBlockingPool(
    const base::FilePath& profile_path) {
  base::FilePath snapshot_file_path;
  base::FilePath media_file_system_dir_path =
      profile_path.AppendASCII(kDeviceMediaAsyncFileUtilTempDir);
  if (!base::CreateDirectory(media_file_system_dir_path) ||
      !base::CreateTemporaryFileInDir(media_file_system_dir_path,
                                      &snapshot_file_path)) {
    LOG(WARNING) << "Could not create media snapshot file "
                 << media_file_system_dir_path.value();
    snapshot_file_path = base::FilePath();
  }
  return snapshot_file_path;
}

// Called after OnDidCreateSnapshotFile finishes media check.
// |callback| is invoked to complete the CreateSnapshotFile request.
void OnDidCheckMediaForCreateSnapshotFile(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const base::File::Info& file_info,
    scoped_refptr<storage::ShareableFileReference> platform_file,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::FilePath platform_path(platform_file.get()->path());
  if (error != base::File::FILE_OK)
    platform_file = NULL;
  callback.Run(error, file_info, platform_path, platform_file);
}

// Called when the snapshot file specified by the |platform_path| is
// successfully created. |file_info| contains the device media file details
// for which the snapshot file is created.
void OnDidCreateSnapshotFile(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::SequencedTaskRunner* media_task_runner,
    bool validate_media_files,
    const base::File::Info& file_info,
    const base::FilePath& platform_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_refptr<storage::ShareableFileReference> file =
      ShareableFileReference::GetOrCreate(
          platform_path,
          ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          media_task_runner);

  if (validate_media_files) {
    base::PostTaskAndReplyWithResult(
        media_task_runner,
        FROM_HERE,
        base::Bind(&NativeMediaFileUtil::IsMediaFile, platform_path),
        base::Bind(&OnDidCheckMediaForCreateSnapshotFile,
                   callback,
                   file_info,
                   file));
  } else {
    OnDidCheckMediaForCreateSnapshotFile(callback, file_info, file,
                                         base::File::FILE_OK);
  }
}

// Called when CreateSnapshotFile method call fails. |callback| is invoked to
// notify the caller about the |error|.
void OnCreateSnapshotFileError(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error, base::File::Info(), base::FilePath(),
               scoped_refptr<ShareableFileReference>());
}

// Called when the snapshot file specified by the |snapshot_file_path| is
// created to hold the contents of the url.path(). If the snapshot
// file is successfully created, |snapshot_file_path| will be an non-empty
// file path. In case of failure, |snapshot_file_path| will be an empty file
// path. Forwards the CreateSnapshot request to the delegate to copy the
// contents of url.path() to |snapshot_file_path|.
void OnSnapshotFileCreatedRunTask(
    std::unique_ptr<FileSystemOperationContext> context,
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const FileSystemURL& url,
    bool validate_media_files,
    const base::FilePath& snapshot_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (snapshot_file_path.empty()) {
    OnCreateSnapshotFileError(callback, base::File::FILE_ERROR_FAILED);
    return;
  }
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  delegate->CreateSnapshotFile(
      url.path(),  // device file path
      snapshot_file_path, base::Bind(&OnDidCreateSnapshotFile, callback,
                                     base::RetainedRef(context->task_runner()),
                                     validate_media_files),
      base::Bind(&OnCreateSnapshotFileError, callback));
}

}  // namespace

class DeviceMediaAsyncFileUtil::MediaPathFilterWrapper
    : public base::RefCountedThreadSafe<MediaPathFilterWrapper> {
 public:
  MediaPathFilterWrapper();

  // Check if entries in |file_list| look like media files.
  // Append the ones that look like media files to |results|.
  // Should run on a media task runner.
  AsyncFileUtil::EntryList FilterMediaEntries(
      const AsyncFileUtil::EntryList& file_list);

  // Check if |path| looks like a media file.
  bool CheckFilePath(const base::FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<MediaPathFilterWrapper>;

  virtual ~MediaPathFilterWrapper();

  std::unique_ptr<MediaPathFilter> media_path_filter_;

  DISALLOW_COPY_AND_ASSIGN(MediaPathFilterWrapper);
};

DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::MediaPathFilterWrapper()
    : media_path_filter_(new MediaPathFilter) {
}

DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::~MediaPathFilterWrapper() {
}

AsyncFileUtil::EntryList
DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::FilterMediaEntries(
    const AsyncFileUtil::EntryList& file_list) {
  AsyncFileUtil::EntryList results;
  for (size_t i = 0; i < file_list.size(); ++i) {
    const storage::DirectoryEntry& entry = file_list[i];
    if (entry.is_directory || CheckFilePath(base::FilePath(entry.name))) {
      results.push_back(entry);
    }
  }
  return results;
}

bool DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::CheckFilePath(
    const base::FilePath& path) {
  return media_path_filter_->Match(path);
}

DeviceMediaAsyncFileUtil::~DeviceMediaAsyncFileUtil() {
}

// static
std::unique_ptr<DeviceMediaAsyncFileUtil> DeviceMediaAsyncFileUtil::Create(
    const base::FilePath& profile_path,
    MediaFileValidationType validation_type) {
  DCHECK(!profile_path.empty());
  return base::WrapUnique(
      new DeviceMediaAsyncFileUtil(profile_path, validation_type));
}

bool DeviceMediaAsyncFileUtil::SupportsStreaming(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate)
    return false;
  return delegate->IsStreaming();
}

void DeviceMediaAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Returns an error if any unsupported flag is found.
  if (file_flags & ~(base::File::FLAG_OPEN |
                     base::File::FLAG_READ |
                     base::File::FLAG_WRITE_ATTRIBUTES)) {
    callback.Run(base::File(base::File::FILE_ERROR_SECURITY), base::Closure());
    return;
  }
  CreateSnapshotFile(
      std::move(context), url,
      base::Bind(&NativeMediaFileUtil::CreatedSnapshotFileForCreateOrOpen,
                 base::RetainedRef(context->task_runner()), file_flags,
                 callback));
}

void DeviceMediaAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY, false);
}

void DeviceMediaAsyncFileUtil::CreateDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateDirectoryError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCreateDirectoryError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }
  delegate->CreateDirectory(
      url.path(), exclusive, recursive,
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCreateDirectory,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&OnCreateDirectoryError, callback));
}

void DeviceMediaAsyncFileUtil::GetFileInfo(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int /* flags */,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnGetFileInfoError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  delegate->GetFileInfo(url.path(),
                        base::Bind(&DeviceMediaAsyncFileUtil::OnDidGetFileInfo,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   base::RetainedRef(context->task_runner()),
                                   url.path(), callback),
                        base::Bind(&OnGetFileInfoError, callback));
}

void DeviceMediaAsyncFileUtil::ReadDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnReadDirectoryError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  delegate->ReadDirectory(
      url.path(),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::RetainedRef(context->task_runner()), callback),
      base::Bind(&OnReadDirectoryError, callback));
}

void DeviceMediaAsyncFileUtil::Touch(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::Truncate(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64_t length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(dest_url);
  if (!delegate) {
    OnCopyFileLocalError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCopyFileLocalError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate->CopyFileLocal(
      src_url.path(), dest_url.path(),
      base::Bind(&CreateSnapshotFileOnBlockingPool, profile_path_),
      progress_callback,
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCopyFileLocal,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&OnCopyFileLocalError, callback));
}

void DeviceMediaAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(dest_url);
  if (!delegate) {
    OnMoveFileLocalError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnMoveFileLocalError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate->MoveFileLocal(
      src_url.path(), dest_url.path(),
      base::Bind(&CreateSnapshotFileOnBlockingPool, profile_path_),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidMoveFileLocal,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&OnMoveFileLocalError, callback));
}

void DeviceMediaAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(dest_url);
  if (!delegate) {
    OnCopyInForeignFileError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCopyInForeignFileError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate->CopyFileFromLocal(
      src_file_path, dest_url.path(),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCopyInForeignFile,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&OnCopyInForeignFileError, callback));
}

void DeviceMediaAsyncFileUtil::DeleteFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* const delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnDeleteFileError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnDeleteFileError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate->DeleteFile(url.path(),
                       base::Bind(&DeviceMediaAsyncFileUtil::OnDidDeleteFile,
                                  weak_ptr_factory_.GetWeakPtr(), callback),
                       base::Bind(&OnDeleteFileError, callback));
}

void DeviceMediaAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* const delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnDeleteDirectoryError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnDeleteDirectoryError(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate->DeleteDirectory(
      url.path(), base::Bind(&DeviceMediaAsyncFileUtil::OnDidDeleteDirectory,
                             weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&OnDeleteDirectoryError, callback));
}

void DeviceMediaAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void DeviceMediaAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner(context->task_runner());
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::Bind(&CreateSnapshotFileOnBlockingPool, profile_path_),
      base::Bind(&OnSnapshotFileCreatedRunTask, base::Passed(&context),
                 callback, url, validate_media_files()));
}

std::unique_ptr<storage::FileStreamReader>
DeviceMediaAsyncFileUtil::GetFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate)
    return std::unique_ptr<storage::FileStreamReader>();

  DCHECK(delegate->IsStreaming());
  return std::unique_ptr<storage::FileStreamReader>(
      new ReadaheadFileStreamReader(new MTPFileStreamReader(
          context, url, offset, expected_modification_time,
          validate_media_files())));
}

void DeviceMediaAsyncFileUtil::AddWatcher(
    const storage::FileSystemURL& url,
    bool recursive,
    const storage::WatcherManager::StatusCallback& callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  MTPDeviceAsyncDelegate* const delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  delegate->AddWatcher(url.origin(), url.path(), recursive, callback,
                       notification_callback);
}

void DeviceMediaAsyncFileUtil::RemoveWatcher(
    const storage::FileSystemURL& url,
    const bool recursive,
    const storage::WatcherManager::StatusCallback& callback) {
  MTPDeviceAsyncDelegate* const delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  delegate->RemoveWatcher(url.origin(), url.path(), recursive, callback);
}

DeviceMediaAsyncFileUtil::DeviceMediaAsyncFileUtil(
    const base::FilePath& profile_path,
    MediaFileValidationType validation_type)
    : profile_path_(profile_path),
      weak_ptr_factory_(this) {
  if (validation_type == APPLY_MEDIA_FILE_VALIDATION) {
    media_path_filter_wrapper_ = new MediaPathFilterWrapper;
  }
}

void DeviceMediaAsyncFileUtil::OnDidCreateDirectory(
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidGetFileInfo(
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& path,
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (file_info.is_directory || !validate_media_files()) {
    OnDidCheckMediaForGetFileInfo(callback, file_info, true /* valid */);
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(&MediaPathFilterWrapper::CheckFilePath,
                 media_path_filter_wrapper_,
                 path),
      base::Bind(&OnDidCheckMediaForGetFileInfo, callback, file_info));
}

void DeviceMediaAsyncFileUtil::OnDidReadDirectory(
    base::SequencedTaskRunner* task_runner,
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    AsyncFileUtil::EntryList file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!validate_media_files()) {
    OnDidCheckMediaForReadDirectory(callback, has_more, std::move(file_list));
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner, FROM_HERE,
      base::BindOnce(&MediaPathFilterWrapper::FilterMediaEntries,
                     media_path_filter_wrapper_, std::move(file_list)),
      base::BindOnce(&OnDidCheckMediaForReadDirectory, callback, has_more));
}

void DeviceMediaAsyncFileUtil::OnDidCopyFileLocal(
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidMoveFileLocal(
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidCopyInForeignFile(
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidDeleteFile(const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidDeleteDirectory(
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_OK);
}

bool DeviceMediaAsyncFileUtil::validate_media_files() const {
  return media_path_filter_wrapper_.get() != NULL;
}
