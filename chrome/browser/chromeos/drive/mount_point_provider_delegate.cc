// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/mount_point_provider_delegate.h"

#include "chrome/browser/chromeos/drive/remote_file_stream_writer.h"
#include "chrome/browser/chromeos/fileapi/remote_file_system_operation.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/remote_file_system_proxy.h"

using content::BrowserThread;

namespace drive {

MountPointProviderDelegate::MountPointProviderDelegate(
    fileapi::ExternalMountPoints* mount_points)
    : mount_points_(mount_points) {
}

MountPointProviderDelegate::~MountPointProviderDelegate() {
}

fileapi::AsyncFileUtil* MountPointProviderDelegate::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, type);

  // TODO(hidehiko): Support this method.
  NOTIMPLEMENTED();
  return NULL;
}

scoped_ptr<webkit_blob::FileStreamReader>
MountPointProviderDelegate::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, url.type());

  fileapi::RemoteFileSystemProxyInterface* proxy =
      mount_points_->GetRemoteFileSystemProxy(url.filesystem_id());
  if (!proxy)
    return scoped_ptr<webkit_blob::FileStreamReader>();

  return proxy->CreateFileStreamReader(
      context->task_runners()->file_task_runner(),
      url, offset, expected_modification_time);
}

scoped_ptr<fileapi::FileStreamWriter>
MountPointProviderDelegate::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, url.type());

  fileapi::RemoteFileSystemProxyInterface* proxy =
      mount_points_->GetRemoteFileSystemProxy(url.filesystem_id());
  if (!proxy)
    return scoped_ptr<fileapi::FileStreamWriter>();

  return scoped_ptr<fileapi::FileStreamWriter>(
      new RemoteFileStreamWriter(
          proxy, url, offset, context->task_runners()->file_task_runner()));
}

fileapi::FileSystemOperation*
MountPointProviderDelegate::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::PlatformFileError* error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, url.type());

  fileapi::RemoteFileSystemProxyInterface* proxy =
      mount_points_->GetRemoteFileSystemProxy(url.filesystem_id());
  if (!proxy) {
    *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return NULL;
  }

  return new chromeos::RemoteFileSystemOperation(proxy);
}

}  // namespace drive
