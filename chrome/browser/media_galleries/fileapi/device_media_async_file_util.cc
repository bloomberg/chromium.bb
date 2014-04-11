// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/fileapi/mtp_file_stream_reader.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/fileapi/readahead_file_stream_reader.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/blob/shareable_file_reference.h"

using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;
using webkit_blob::ShareableFileReference;

namespace {

const char kDeviceMediaAsyncFileUtilTempDir[] = "DeviceMediaFileSystem";

// Called on the IO thread.
MTPDeviceAsyncDelegate* GetMTPDeviceDelegate(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(
      url.filesystem_id());
}

// Called on a blocking pool thread to create a snapshot file to hold the
// contents of |device_file_path|. The snapshot file is created in the
// "profile_path/kDeviceMediaAsyncFileUtilTempDir" directory. Return the
// snapshot file path or an empty path on failure.
base::FilePath CreateSnapshotFileOnBlockingPool(
    const base::FilePath& device_file_path,
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

}  // namespace

DeviceMediaAsyncFileUtil::~DeviceMediaAsyncFileUtil() {
}

// static
DeviceMediaAsyncFileUtil* DeviceMediaAsyncFileUtil::Create(
    const base::FilePath& profile_path) {
  DCHECK(!profile_path.empty());
  return new DeviceMediaAsyncFileUtil(profile_path);
}

bool DeviceMediaAsyncFileUtil::SupportsStreaming(
    const fileapi::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate)
    return false;
  return delegate->IsStreaming();
}

void DeviceMediaAsyncFileUtil::CreateOrOpen(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Returns an error if any unsupported flag is found.
  if (file_flags & ~(base::File::FLAG_OPEN |
                     base::File::FLAG_READ |
                     base::File::FLAG_WRITE_ATTRIBUTES)) {
    base::PlatformFile invalid_file(base::kInvalidPlatformFileValue);
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 base::PassPlatformFile(&invalid_file),
                 base::Closure());
    return;
  }
  CreateSnapshotFile(
      context.Pass(),
      url,
      base::Bind(&NativeMediaFileUtil::CreatedSnapshotFileForCreateOrOpen,
                 make_scoped_refptr(context->task_runner()),
                 file_flags,
                 callback));
}

void DeviceMediaAsyncFileUtil::EnsureFileExists(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY, false);
}

void DeviceMediaAsyncFileUtil::CreateDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::GetFileInfo(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnGetFileInfoError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  delegate->GetFileInfo(
      url.path(),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&DeviceMediaAsyncFileUtil::OnGetFileInfoError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DeviceMediaAsyncFileUtil::ReadDirectory(
    scoped_ptr<FileSystemOperationContext> context,
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
                 callback),
      base::Bind(&DeviceMediaAsyncFileUtil::OnReadDirectoryError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DeviceMediaAsyncFileUtil::Touch(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::Truncate(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::MoveFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyInForeignFile(
    scoped_ptr<FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteRecursively(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void DeviceMediaAsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Grab the SequencedTaskRunner now because base::Passed(&context) will
  // turn |context| empty before |task_runner| gets accessed.
  scoped_refptr<base::SequencedTaskRunner> task_runner(context->task_runner());
  base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(&CreateSnapshotFileOnBlockingPool,
                 url.path(),
                 profile_path_),
      base::Bind(&DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&context),
                 callback,
                 url));
}

scoped_ptr<webkit_blob::FileStreamReader>
DeviceMediaAsyncFileUtil::GetFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) {
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate)
    return scoped_ptr<webkit_blob::FileStreamReader>();

  DCHECK(delegate->IsStreaming());
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new ReadaheadFileStreamReader(new MTPFileStreamReader(
          context, url, offset, expected_modification_time)));
}

DeviceMediaAsyncFileUtil::DeviceMediaAsyncFileUtil(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      weak_ptr_factory_(this) {
}

void DeviceMediaAsyncFileUtil::OnDidGetFileInfo(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::File::Info& file_info) {
  callback.Run(base::File::FILE_OK, file_info);
}

void DeviceMediaAsyncFileUtil::OnGetFileInfoError(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    base::File::Error error) {
  callback.Run(error, base::File::Info());
}

void DeviceMediaAsyncFileUtil::OnDidReadDirectory(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    const AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  callback.Run(base::File::FILE_OK, file_list, has_more);
}

void DeviceMediaAsyncFileUtil::OnReadDirectoryError(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    base::File::Error error) {
  callback.Run(error, AsyncFileUtil::EntryList(), false /*no more*/);
}

void DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::SequencedTaskRunner* media_task_runner,
    const base::File::Info& file_info,
    const base::FilePath& platform_path) {
  base::PostTaskAndReplyWithResult(
      media_task_runner,
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::IsMediaFile, platform_path),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCheckMedia,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_info,
                 ShareableFileReference::GetOrCreate(
                     platform_path,
                     ShareableFileReference::DELETE_ON_FINAL_RELEASE,
                     media_task_runner)));
}

void DeviceMediaAsyncFileUtil::OnDidCheckMedia(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const base::File::Info& file_info,
    scoped_refptr<webkit_blob::ShareableFileReference> platform_file,
    base::File::Error error) {
  base::FilePath platform_path(platform_file.get()->path());
  if (error != base::File::FILE_OK)
    platform_file = NULL;
  callback.Run(error, file_info, platform_path, platform_file);
}

void DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::File::Error error) {
  callback.Run(error, base::File::Info(), base::FilePath(),
               scoped_refptr<ShareableFileReference>());
}

void DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask(
    scoped_ptr<FileSystemOperationContext> context,
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const FileSystemURL& url,
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
      snapshot_file_path,
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 make_scoped_refptr(context->task_runner())),
      base::Bind(&DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}
