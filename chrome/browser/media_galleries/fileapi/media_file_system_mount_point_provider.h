// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_file_system_config.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

namespace fileapi {
class AsyncFileUtilAdapter;
}

namespace chrome {

class MediaPathFilter;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
class DeviceMediaAsyncFileUtil;
#endif

class MediaFileSystemMountPointProvider
    : public fileapi::FileSystemMountPointProvider {
 public:
  static const char kMediaPathFilterKey[];
  static const char kMTPDeviceDelegateURLKey[];

  explicit MediaFileSystemMountPointProvider(
      const base::FilePath& profile_path);
  virtual ~MediaFileSystemMountPointProvider();

  // FileSystemMountPointProvider implementation.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual base::FilePath GetFileSystemRootPathOnFileThread(
      const fileapi::FileSystemURL& url,
      bool create) OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
  GetCopyOrMoveFileValidatorFactory(
      fileapi::FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
  virtual void InitializeCopyOrMoveFileValidatorFactory(
      fileapi::FileSystemType type,
      scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory> factory) OVERRIDE;
  virtual fileapi::FilePermissionPolicy GetPermissionPolicy(
      const fileapi::FileSystemURL& url, int permissions) const OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;
  virtual void DeleteFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::FileSystemContext* context,
      const DeleteFileSystemCallback& callback) OVERRIDE;

 private:
  // Store the profile path. We need this to create temporary snapshot files.
  const base::FilePath profile_path_;

  scoped_ptr<MediaPathFilter> media_path_filter_;
  scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory>
      media_copy_or_move_file_validator_factory_;

  scoped_ptr<fileapi::AsyncFileUtilAdapter> native_media_file_util_;
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  scoped_ptr<DeviceMediaAsyncFileUtil> device_media_async_file_util_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemMountPointProvider);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
