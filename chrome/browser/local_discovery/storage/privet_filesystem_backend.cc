// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_backend.h"

#include <string>

#include "chrome/browser/local_discovery/storage/privet_filesystem_async_util.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"
#include "webkit/browser/fileapi/file_system_operation.h"

namespace local_discovery {

PrivetFileSystemBackend::PrivetFileSystemBackend(
    fileapi::ExternalMountPoints* mount_points)
    : mount_points_(mount_points),
      async_util_(new PrivetFileSystemAsyncUtil()) {
}

PrivetFileSystemBackend::~PrivetFileSystemBackend() {
}

bool PrivetFileSystemBackend::CanHandleType(
    fileapi::FileSystemType type) const {
  return (type == fileapi::kFileSystemTypeCloudDevice);
}

void PrivetFileSystemBackend::Initialize(fileapi::FileSystemContext* context) {
  mount_points_->RegisterFileSystem(
      "privet",
      fileapi::kFileSystemTypeCloudDevice,
      fileapi::FileSystemMountOption(),
      base::FilePath(kPrivetFilePath));
}

void PrivetFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // Copied from src/chrome/browser/chromeos/fileapi/file_system_backend.cc
  // This is deprecated for non-sandboxed filesystems.
  NOTREACHED();
  callback.Run(GURL(), std::string(), base::File::FILE_ERROR_SECURITY);
}

fileapi::FileSystemQuotaUtil* PrivetFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

fileapi::AsyncFileUtil* PrivetFileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  return async_util_.get();
}

fileapi::CopyOrMoveFileValidatorFactory*
PrivetFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type, base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

fileapi::FileSystemOperation*
PrivetFileSystemBackend::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::File::Error* error_code) const {
  return fileapi::FileSystemOperation::Create(
      url, context,
      make_scoped_ptr(new fileapi::FileSystemOperationContext(context)));
}

scoped_ptr<webkit_blob::FileStreamReader>
PrivetFileSystemBackend::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>();
}

scoped_ptr<fileapi::FileStreamWriter>
PrivetFileSystemBackend::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>();
}

}  // namespace local_discovery
