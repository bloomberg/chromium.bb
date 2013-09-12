// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_system_backend.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/fileapi/file_access_permissions.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/isolated_file_util.h"
#include "webkit/browser/fileapi/local_file_stream_writer.h"

namespace {

const char kChromeUIScheme[] = "chrome";

}  // namespace

namespace chromeos {

// static
bool FileSystemBackend::CanHandleURL(const fileapi::FileSystemURL& url) {
  if (!url.is_valid())
    return false;
  return url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == fileapi::kFileSystemTypeDrive;
}

FileSystemBackend::FileSystemBackend(
    FileSystemBackendDelegate* drive_delegate,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<fileapi::ExternalMountPoints> mount_points,
    fileapi::ExternalMountPoints* system_mount_points)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(new fileapi::AsyncFileUtilAdapter(
          new fileapi::IsolatedFileUtil())),
      drive_delegate_(drive_delegate),
      mount_points_(mount_points),
      system_mount_points_(system_mount_points) {
}

FileSystemBackend::~FileSystemBackend() {
}

void FileSystemBackend::AddSystemMountPoints() {
  // RegisterFileSystem() is no-op if the mount point with the same name
  // already exists, hence it's safe to call without checking if a mount
  // point already exists or not.

  // TODO(satorux): "Downloads" directory should probably be per-profile. For
  // this to be per-profile, a unique directory path should be chosen per
  // profile, and the mount point should be added to
  // mount_points_. crbug.com/247236
  base::FilePath home_path;
  if (PathService::Get(base::DIR_HOME, &home_path)) {
    system_mount_points_->RegisterFileSystem(
        "Downloads",
        fileapi::kFileSystemTypeNativeLocal,
        home_path.AppendASCII("Downloads"));
  }

  system_mount_points_->RegisterFileSystem(
      "archive",
      fileapi::kFileSystemTypeNativeLocal,
      chromeos::CrosDisksClient::GetArchiveMountPoint());
  system_mount_points_->RegisterFileSystem(
      "removable",
      fileapi::kFileSystemTypeNativeLocal,
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint());
  system_mount_points_->RegisterFileSystem(
      "oem",
      fileapi::kFileSystemTypeRestrictedNativeLocal,
      base::FilePath(FILE_PATH_LITERAL("/usr/share/oem")));
}

bool FileSystemBackend::CanHandleType(fileapi::FileSystemType type) const {
  switch (type) {
    case fileapi::kFileSystemTypeExternal:
    case fileapi::kFileSystemTypeDrive:
    case fileapi::kFileSystemTypeRestrictedNativeLocal:
    case fileapi::kFileSystemTypeNativeLocal:
    case fileapi::kFileSystemTypeNativeForPlatformApp:
      return true;
    default:
      return false;
  }
}

void FileSystemBackend::Initialize(fileapi::FileSystemContext* context) {
}

void FileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(fileapi::IsolatedContext::IsIsolatedType(type));
  // Nothing to validate for external filesystem.
  callback.Run(GetFileSystemRootURI(origin_url, type),
               GetFileSystemName(origin_url, type),
               base::PLATFORM_FILE_OK);
}

fileapi::FileSystemQuotaUtil* FileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

bool FileSystemBackend::IsAccessAllowed(
    const fileapi::FileSystemURL& url) const {
  if (!url.is_valid())
    return false;

  // Permit access to mount points from internal WebUI.
  const GURL& origin_url = url.origin();
  if (origin_url.SchemeIs(kChromeUIScheme))
    return true;

  // No extra check is needed for isolated file systems.
  if (url.mount_type() == fileapi::kFileSystemTypeIsolated)
    return true;

  if (!CanHandleURL(url))
    return false;

  std::string extension_id = origin_url.host();
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

  std::vector<fileapi::MountPoints::MountPointInfo> files;
  mount_points_->AddMountPointInfosTo(&files);
  system_mount_points_->AddMountPointInfosTo(&files);

  for (size_t i = 0; i < files.size(); ++i) {
    file_access_permissions_->GrantAccessPermission(
        extension_id,
        base::FilePath::FromUTF8Unsafe(files[i].name));
  }
}

void FileSystemBackend::GrantFileAccessToExtension(
    const std::string& extension_id, const base::FilePath& virtual_path) {
  // All we care about here is access from extensions for now.
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;

  std::string id;
  fileapi::FileSystemType type;
  base::FilePath path;
  if (!mount_points_->CrackVirtualPath(virtual_path, &id, &type, &path) &&
      !system_mount_points_->CrackVirtualPath(virtual_path,
                                              &id, &type, &path)) {
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
  if (type == fileapi::kFileSystemTypeDrive)
    return drive_delegate_->GetAsyncFileUtil(type);

  DCHECK(type == fileapi::kFileSystemTypeNativeLocal ||
         type == fileapi::kFileSystemTypeRestrictedNativeLocal);
  return local_file_util_.get();
}

fileapi::CopyOrMoveFileValidatorFactory*
FileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  return NULL;
}

fileapi::FileSystemOperation* FileSystemBackend::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url)) {
    *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
    return NULL;
  }

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == fileapi::kFileSystemTypeDrive);
  return fileapi::FileSystemOperation::Create(
      url, context,
      make_scoped_ptr(new fileapi::FileSystemOperationContext(context)));
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

  if (url.type() == fileapi::kFileSystemTypeDrive) {
    return drive_delegate_->CreateFileStreamReader(
        url, offset, expected_modification_time, context);
  }

  return scoped_ptr<webkit_blob::FileStreamReader>(
      new fileapi::FileSystemFileStreamReader(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
FileSystemBackend::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url))
    return scoped_ptr<fileapi::FileStreamWriter>();

  if (url.type() == fileapi::kFileSystemTypeDrive)
    return drive_delegate_->CreateFileStreamWriter(url, offset, context);

  if (url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal)
    return scoped_ptr<fileapi::FileStreamWriter>();

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal);
  return scoped_ptr<fileapi::FileStreamWriter>(
      new fileapi::LocalFileStreamWriter(
          context->default_file_task_runner(), url.path(), offset));
}

bool FileSystemBackend::GetVirtualPath(
    const base::FilePath& filesystem_path,
    base::FilePath* virtual_path) {
  return mount_points_->GetVirtualPath(filesystem_path, virtual_path) ||
         system_mount_points_->GetVirtualPath(filesystem_path, virtual_path);
}

}  // namespace chromeos
