// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_backend.h"

#include <string>

#include "chrome/browser/local_discovery/storage/privet_filesystem_async_util.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace local_discovery {

PrivetFileSystemBackend::PrivetFileSystemBackend(
    storage::ExternalMountPoints* mount_points,
    content::BrowserContext* browser_context)
    : mount_points_(mount_points),
      async_util_(new PrivetFileSystemAsyncUtil(browser_context)) {
}

PrivetFileSystemBackend::~PrivetFileSystemBackend() {
}

bool PrivetFileSystemBackend::CanHandleType(
    storage::FileSystemType type) const {
  return (type == storage::kFileSystemTypeCloudDevice);
}

void PrivetFileSystemBackend::Initialize(storage::FileSystemContext* context) {
  mount_points_->RegisterFileSystem("privet",
                                    storage::kFileSystemTypeCloudDevice,
                                    storage::FileSystemMountOption(),
                                    base::FilePath(kPrivetFilePath));
}

void PrivetFileSystemBackend::ResolveURL(
    const storage::FileSystemURL& url,
    storage::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // TODO(noamsml): Provide a proper root url and a proper name.
  GURL root_url = GURL(
      storage::GetExternalFileSystemRootURIString(url.origin(), std::string()));
  callback.Run(root_url, std::string(), base::File::FILE_OK);
}

storage::FileSystemQuotaUtil* PrivetFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

storage::AsyncFileUtil* PrivetFileSystemBackend::GetAsyncFileUtil(
    storage::FileSystemType type) {
  return async_util_.get();
}

storage::CopyOrMoveFileValidatorFactory*
PrivetFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    storage::FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

storage::FileSystemOperation*
PrivetFileSystemBackend::CreateFileSystemOperation(
    const storage::FileSystemURL& url,
    storage::FileSystemContext* context,
    base::File::Error* error_code) const {
  return storage::FileSystemOperation::Create(
      url,
      context,
      make_scoped_ptr(new storage::FileSystemOperationContext(context)));
}

bool PrivetFileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  return false;
}

bool PrivetFileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  return true;
}

scoped_ptr<storage::FileStreamReader>
PrivetFileSystemBackend::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) const {
  return scoped_ptr<storage::FileStreamReader>();
}

scoped_ptr<storage::FileStreamWriter>
PrivetFileSystemBackend::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64 offset,
    storage::FileSystemContext* context) const {
  return scoped_ptr<storage::FileStreamWriter>();
}

}  // namespace local_discovery
