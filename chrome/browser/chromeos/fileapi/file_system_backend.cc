// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_system_backend.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/fileapi/file_access_permissions.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace chromeos {

// static
bool FileSystemBackend::CanHandleURL(const fileapi::FileSystemURL& url) {
  if (!url.is_valid())
    return false;
  return url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == fileapi::kFileSystemTypeDrive ||
         url.type() == fileapi::kFileSystemTypeProvided ||
         url.type() == fileapi::kFileSystemTypeDeviceMediaAsFileStorage;
}

FileSystemBackend::FileSystemBackend(
    FileSystemBackendDelegate* drive_delegate,
    FileSystemBackendDelegate* file_system_provider_delegate,
    FileSystemBackendDelegate* mtp_delegate,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<fileapi::ExternalMountPoints> mount_points,
    fileapi::ExternalMountPoints* system_mount_points)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(fileapi::AsyncFileUtil::CreateForLocalFileSystem()),
      drive_delegate_(drive_delegate),
      file_system_provider_delegate_(file_system_provider_delegate),
      mtp_delegate_(mtp_delegate),
      mount_points_(mount_points),
      system_mount_points_(system_mount_points) {}

FileSystemBackend::~FileSystemBackend() {
}

void FileSystemBackend::AddSystemMountPoints() {
  // RegisterFileSystem() is no-op if the mount point with the same name
  // already exists, hence it's safe to call without checking if a mount
  // point already exists or not.
  system_mount_points_->RegisterFileSystem(
      "archive",
      fileapi::kFileSystemTypeNativeLocal,
      fileapi::FileSystemMountOption(),
      chromeos::CrosDisksClient::GetArchiveMountPoint());
  system_mount_points_->RegisterFileSystem(
      "removable",
      fileapi::kFileSystemTypeNativeLocal,
      fileapi::FileSystemMountOption(fileapi::COPY_SYNC_OPTION_SYNC),
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint());
  system_mount_points_->RegisterFileSystem(
      "oem",
      fileapi::kFileSystemTypeRestrictedNativeLocal,
      fileapi::FileSystemMountOption(),
      base::FilePath(FILE_PATH_LITERAL("/usr/share/oem")));
}

bool FileSystemBackend::CanHandleType(fileapi::FileSystemType type) const {
  switch (type) {
    case fileapi::kFileSystemTypeExternal:
    case fileapi::kFileSystemTypeDrive:
    case fileapi::kFileSystemTypeRestrictedNativeLocal:
    case fileapi::kFileSystemTypeNativeLocal:
    case fileapi::kFileSystemTypeNativeForPlatformApp:
    case fileapi::kFileSystemTypeDeviceMediaAsFileStorage:
    case fileapi::kFileSystemTypeProvided:
      return true;
    default:
      return false;
  }
}

void FileSystemBackend::Initialize(fileapi::FileSystemContext* context) {
}

void FileSystemBackend::ResolveURL(const fileapi::FileSystemURL& url,
                                   fileapi::OpenFileSystemMode mode,
                                   const OpenFileSystemCallback& callback) {
  std::string id;
  fileapi::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  fileapi::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(
           url.virtual_path(), &id, &type, &cracked_id, &path, &option) &&
      !system_mount_points_->CrackVirtualPath(
           url.virtual_path(), &id, &type, &cracked_id, &path, &option)) {
    // Not under a mount point, so return an error, since the root is not
    // accessible.
    GURL root_url = GURL(fileapi::GetExternalFileSystemRootURIString(
        url.origin(), std::string()));
    callback.Run(root_url, std::string(), base::File::FILE_ERROR_SECURITY);
    return;
  }

  std::string name;
  // Construct a URL restricted to the found mount point.
  std::string root_url =
      fileapi::GetExternalFileSystemRootURIString(url.origin(), id);

  // For removable and archives, the file system root is the external mount
  // point plus the inner mount point.
  if (id == "archive" || id == "removable") {
    std::vector<std::string> components;
    url.virtual_path().GetComponents(&components);
    DCHECK_EQ(id, components.at(0));
    if (components.size() < 2) {
      // Unable to access /archive and /removable directories directly. The
      // inner mount name must be specified.
      callback.Run(
          GURL(root_url), std::string(), base::File::FILE_ERROR_SECURITY);
      return;
    }
    std::string inner_mount_name = components[1];
    root_url += inner_mount_name + "/";
    name = inner_mount_name;
  } else {
    name = id;
  }

  callback.Run(GURL(root_url), name, base::File::FILE_OK);
}

fileapi::FileSystemQuotaUtil* FileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

bool FileSystemBackend::IsAccessAllowed(
    const fileapi::FileSystemURL& url) const {
  if (!url.is_valid())
    return false;

  // No extra check is needed for isolated file systems.
  if (url.mount_type() == fileapi::kFileSystemTypeIsolated)
    return true;

  if (!CanHandleURL(url))
    return false;

  std::string extension_id = url.origin().host();
  // TODO(mtomasz): Temporarily whitelist TimeScapes. Remove this in M-31.
  // See: crbug.com/271946
  if (extension_id == "mlbmkoenclnokonejhlfakkeabdlmpek" &&
      url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal) {
    return true;
  }

  // Check first to make sure this extension has fileBrowserHander permissions.
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return false;

  return file_access_permissions_->HasAccessPermission(extension_id,
                                                       url.virtual_path());
}

void FileSystemBackend::GrantFullAccessToExtension(
    const std::string& extension_id) {
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;
  file_access_permissions_->GrantFullAccessPermission(extension_id);
}

void FileSystemBackend::GrantFileAccessToExtension(
    const std::string& extension_id, const base::FilePath& virtual_path) {
  // All we care about here is access from extensions for now.
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;

  std::string id;
  fileapi::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  fileapi::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(virtual_path, &id, &type, &cracked_id,
                                       &path, &option) &&
      !system_mount_points_->CrackVirtualPath(virtual_path, &id, &type,
                                              &cracked_id, &path, &option)) {
    return;
  }

  if (type == fileapi::kFileSystemTypeRestrictedNativeLocal) {
    LOG(ERROR) << "Can't grant access for restricted mount point";
    return;
  }

  file_access_permissions_->GrantAccessPermission(extension_id, virtual_path);
}

void FileSystemBackend::RevokeAccessForExtension(
      const std::string& extension_id) {
  file_access_permissions_->RevokePermissions(extension_id);
}

std::vector<base::FilePath> FileSystemBackend::GetRootDirectories() const {
  std::vector<fileapi::MountPoints::MountPointInfo> mount_points;
  mount_points_->AddMountPointInfosTo(&mount_points);
  system_mount_points_->AddMountPointInfosTo(&mount_points);

  std::vector<base::FilePath> root_dirs;
  for (size_t i = 0; i < mount_points.size(); ++i)
    root_dirs.push_back(mount_points[i].path);
  return root_dirs;
}

fileapi::AsyncFileUtil* FileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  switch (type) {
    case fileapi::kFileSystemTypeDrive:
      return drive_delegate_->GetAsyncFileUtil(type);
    case fileapi::kFileSystemTypeProvided:
      return file_system_provider_delegate_->GetAsyncFileUtil(type);
    case fileapi::kFileSystemTypeNativeLocal:
    case fileapi::kFileSystemTypeRestrictedNativeLocal:
      return local_file_util_.get();
    case fileapi::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->GetAsyncFileUtil(type);
    default:
      NOTREACHED();
  }
  return NULL;
}

fileapi::CopyOrMoveFileValidatorFactory*
FileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type, base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

fileapi::FileSystemOperation* FileSystemBackend::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::File::Error* error_code) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return NULL;
  }

  if (url.type() == fileapi::kFileSystemTypeDeviceMediaAsFileStorage) {
    // MTP file operations run on MediaTaskRunner.
    return fileapi::FileSystemOperation::Create(
        url, context,
        make_scoped_ptr(new fileapi::FileSystemOperationContext(
            context, MediaFileSystemBackend::MediaTaskRunner())));
  }

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == fileapi::kFileSystemTypeDrive ||
         url.type() == fileapi::kFileSystemTypeProvided);
  return fileapi::FileSystemOperation::Create(
      url, context,
      make_scoped_ptr(new fileapi::FileSystemOperationContext(context)));
}

bool FileSystemBackend::SupportsStreaming(
    const fileapi::FileSystemURL& url) const {
  return url.type() == fileapi::kFileSystemTypeDrive ||
         url.type() == fileapi::kFileSystemTypeProvided ||
         url.type() == fileapi::kFileSystemTypeDeviceMediaAsFileStorage;
}

scoped_ptr<webkit_blob::FileStreamReader>
FileSystemBackend::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url))
    return scoped_ptr<webkit_blob::FileStreamReader>();

  switch (url.type()) {
    case fileapi::kFileSystemTypeDrive:
      return drive_delegate_->CreateFileStreamReader(
          url, offset, expected_modification_time, context);
    case fileapi::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamReader(
          url, offset, expected_modification_time, context);
    case fileapi::kFileSystemTypeNativeLocal:
    case fileapi::kFileSystemTypeRestrictedNativeLocal:
      return scoped_ptr<webkit_blob::FileStreamReader>(
          webkit_blob::FileStreamReader::CreateForFileSystemFile(
              context, url, offset, expected_modification_time));
    case fileapi::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamReader(
          url, offset, expected_modification_time, context);
    default:
      NOTREACHED();
  }
  return scoped_ptr<webkit_blob::FileStreamReader>();
}

scoped_ptr<fileapi::FileStreamWriter>
FileSystemBackend::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url))
    return scoped_ptr<fileapi::FileStreamWriter>();

  switch (url.type()) {
    case fileapi::kFileSystemTypeDrive:
      return drive_delegate_->CreateFileStreamWriter(url, offset, context);
    case fileapi::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamWriter(
          url, offset, context);
    case fileapi::kFileSystemTypeNativeLocal:
      return scoped_ptr<fileapi::FileStreamWriter>(
          fileapi::FileStreamWriter::CreateForLocalFile(
              context->default_file_task_runner(), url.path(), offset,
              fileapi::FileStreamWriter::OPEN_EXISTING_FILE));
    case fileapi::kFileSystemTypeRestrictedNativeLocal:
      // Restricted native local file system is read only.
      return scoped_ptr<fileapi::FileStreamWriter>();
    case fileapi::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamWriter(url, offset, context);
    default:
      NOTREACHED();
  }
  return scoped_ptr<fileapi::FileStreamWriter>();
}

bool FileSystemBackend::GetVirtualPath(
    const base::FilePath& filesystem_path,
    base::FilePath* virtual_path) {
  return mount_points_->GetVirtualPath(filesystem_path, virtual_path) ||
         system_mount_points_->GetVirtualPath(filesystem_path, virtual_path);
}

}  // namespace chromeos
