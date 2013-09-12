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
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;
using webkit_blob::ShareableFileReference;

namespace chrome {

namespace {

const char kDeviceMediaAsyncFileUtilTempDir[] = "DeviceMediaFileSystem";

// Called on the IO thread.
MTPDeviceAsyncDelegate* GetMTPDeviceDelegate(const FileSystemURL& url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(
      url.filesystem_id());
}

// Called on a blocking pool thread to create a snapshot file to hold the
// contents of |device_file_path|. The snapshot file is created in
// "profile_path/kDeviceMediaAsyncFileUtilTempDir" directory. If the snapshot
// file is created successfully, |snapshot_file_path| will be a non-empty file
// path. In case of failure, the |snapshot_file_path| will be an empty file
// path.
void CreateSnapshotFileOnBlockingPool(
    const base::FilePath& device_file_path,
    const base::FilePath& profile_path,
    base::FilePath* snapshot_file_path) {
  DCHECK(snapshot_file_path);
  base::FilePath isolated_media_file_system_dir_path =
      profile_path.AppendASCII(kDeviceMediaAsyncFileUtilTempDir);
  if (!file_util::CreateDirectory(isolated_media_file_system_dir_path) ||
      !file_util::CreateTemporaryFileInDir(isolated_media_file_system_dir_path,
                                           snapshot_file_path)) {
    LOG(WARNING) << "Could not create media snapshot file "
                 << isolated_media_file_system_dir_path.value();
    *snapshot_file_path = base::FilePath();
  }
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

void DeviceMediaAsyncFileUtil::CreateOrOpen(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  base::PlatformFile invalid_file = base::kInvalidPlatformFileValue;
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY,
               base::PassPlatformFile(&invalid_file),
               base::Closure());
}

void DeviceMediaAsyncFileUtil::EnsureFileExists(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY, false);
}

void DeviceMediaAsyncFileUtil::CreateDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::GetFileInfo(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnGetFileInfoError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnReadDirectoryError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::Truncate(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::MoveFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyInForeignFile(
    scoped_ptr<FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NOTIMPLEMENTED();
  callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::DeleteRecursively(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

void DeviceMediaAsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  base::FilePath* snapshot_file_path = new base::FilePath;
  base::SequencedTaskRunner* task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&CreateSnapshotFileOnBlockingPool,
                     url.path(),
                     profile_path_,
                     base::Unretained(snapshot_file_path)),
          base::Bind(&DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&context),
                     callback,
                     url,
                     base::Owned(snapshot_file_path)));
  DCHECK(success);
}

DeviceMediaAsyncFileUtil::DeviceMediaAsyncFileUtil(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      weak_ptr_factory_(this) {
}

void DeviceMediaAsyncFileUtil::OnDidGetFileInfo(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::PlatformFileInfo& file_info) {
  callback.Run(base::PLATFORM_FILE_OK, file_info);
}

void DeviceMediaAsyncFileUtil::OnGetFileInfoError(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    base::PlatformFileError error) {
  callback.Run(error, base::PlatformFileInfo());
}

void DeviceMediaAsyncFileUtil::OnDidReadDirectory(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    const AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  callback.Run(base::PLATFORM_FILE_OK, file_list, has_more);
}

void DeviceMediaAsyncFileUtil::OnReadDirectoryError(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    base::PlatformFileError error) {
  callback.Run(error, AsyncFileUtil::EntryList(), false /*no more*/);
}

void DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::SequencedTaskRunner* media_task_runner,
    const base::PlatformFileInfo& file_info,
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
    const base::PlatformFileInfo& file_info,
    scoped_refptr<webkit_blob::ShareableFileReference> platform_file,
    base::PlatformFileError error) {
  base::FilePath platform_path(platform_file.get()->path());
  if (error != base::PLATFORM_FILE_OK)
    platform_file = NULL;
  callback.Run(error, file_info, platform_path, platform_file);
}

void DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::PlatformFileError error) {
  callback.Run(error, base::PlatformFileInfo(), base::FilePath(),
               scoped_refptr<ShareableFileReference>());
}

void DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask(
    scoped_ptr<FileSystemOperationContext> context,
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const FileSystemURL& url,
    base::FilePath* snapshot_file_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!snapshot_file_path || snapshot_file_path->empty()) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  delegate->CreateSnapshotFile(
      url.path(),  // device file path
      *snapshot_file_path,
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 make_scoped_refptr(context->task_runner())),
      base::Bind(&DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

}  // namespace chrome
