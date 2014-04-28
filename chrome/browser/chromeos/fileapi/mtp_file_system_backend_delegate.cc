// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace chromeos {

MTPFileSystemBackendDelegate::MTPFileSystemBackendDelegate(
    const base::FilePath& storage_partition_path)
    : device_media_async_file_util_(
          DeviceMediaAsyncFileUtil::Create(storage_partition_path,
                                           NO_MEDIA_FILE_VALIDATION)) {
}

MTPFileSystemBackendDelegate::~MTPFileSystemBackendDelegate() {
}

fileapi::AsyncFileUtil* MTPFileSystemBackendDelegate::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK_EQ(fileapi::kFileSystemTypeDeviceMediaAsFileStorage, type);

  return device_media_async_file_util_.get();
}

scoped_ptr<webkit_blob::FileStreamReader>
MTPFileSystemBackendDelegate::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) {
  DCHECK_EQ(fileapi::kFileSystemTypeDeviceMediaAsFileStorage, url.type());

  return device_media_async_file_util_->GetFileStreamReader(
      url, offset, expected_modification_time, context);
}

scoped_ptr<fileapi::FileStreamWriter>
MTPFileSystemBackendDelegate::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) {
  DCHECK_EQ(fileapi::kFileSystemTypeDeviceMediaAsFileStorage, url.type());

  // TODO(kinaba): support writing.
  return scoped_ptr<fileapi::FileStreamWriter>();
}

}  // namespace chromeos
