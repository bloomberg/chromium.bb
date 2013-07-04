// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_MOUNT_POINT_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_MOUNT_POINT_PROVIDER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/fileapi/cros_mount_point_provider_delegate.h"

namespace fileapi {
class ExternalMountPoints;
}  // namespace fileapi

namespace drive {

// Delegate implementation of the some methods in CrosMountPointProvider
// for Drive file system.
class MountPointProviderDelegate
    : public chromeos::CrosMountPointProviderDelegate {
 public:
  explicit MountPointProviderDelegate(
      fileapi::ExternalMountPoints* mount_points);
  virtual ~MountPointProviderDelegate();

  // CrosMountPointProvider::Delegate overrides.
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) OVERRIDE;

 private:
  scoped_refptr<fileapi::ExternalMountPoints> mount_points_;

  DISALLOW_COPY_AND_ASSIGN(MountPointProviderDelegate);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_MOUNT_POINT_PROVIDER_DELEGATE_H_
