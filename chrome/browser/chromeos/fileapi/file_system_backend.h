// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace fileapi {
class CopyOrMoveFileValidatorFactory;
class ExternalMountPoints;
class FileSystemURL;
}  // namespace fileapi

namespace chromeos {

class FileSystemBackendDelegate;
class FileAccessPermissions;

// FileSystemBackend is a Chrome OS specific implementation of
// ExternalFileSystemBackend. This class is responsible for a
// number of things, including:
//
// - Add system mount points
// - Grant/revoke/check file access permissions
// - Create FileSystemOperation per file system type
// - Create FileStreamReader/Writer per file system type
//
// Chrome OS specific mount points:
//
// "Downloads" is a mount point for user's Downloads directory on the local
// disk, where downloaded files are stored by default.
//
// "archive" is a mount point for an archive file, such as a zip file. This
// mount point exposes contents of an archive file via cros_disks and AVFS
// <http://avf.sourceforge.net/>.
//
// "removable" is a mount point for removable media such as an SD card.
// Insertion and removal of removable media are handled by cros_disks.
//
// "oem" is a read-only mount point for a directory containing OEM data.
//
// "drive" is a mount point for Google Drive. Drive is integrated with the
// FileSystem API layer via drive::FileSystemProxy. This mount point is added
// by drive::DriveIntegrationService.
//
// These mount points are placed under the "external" namespace, and file
// system URLs for these mount points look like:
//
//   filesystem:<origin>/external/<mount_name>/...
//
class FileSystemBackend : public fileapi::ExternalFileSystemBackend {
 public:
  using fileapi::FileSystemBackend::OpenFileSystemCallback;

  // FileSystemBackend will take an ownership of a |mount_points|
  // reference. On the other hand, |system_mount_points| will be kept as a raw
  // pointer and it should outlive FileSystemBackend instance.
  // The ownerships of |drive_delegate| and |file_system_provider_delegate| are
  // also taken.
  FileSystemBackend(
      FileSystemBackendDelegate* drive_delegate,
      FileSystemBackendDelegate* file_system_provider_delegate,
      FileSystemBackendDelegate* mtp_delegate,
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<fileapi::ExternalMountPoints> mount_points,
      fileapi::ExternalMountPoints* system_mount_points);
  virtual ~FileSystemBackend();

  // Adds system mount points, such as "archive", and "removable". This
  // function is no-op if these mount points are already present.
  void AddSystemMountPoints();

  // Returns true if CrosMountpointProvider can handle |url|, i.e. its
  // file system type matches with what this provider supports.
  // This could be called on any threads.
  static bool CanHandleURL(const fileapi::FileSystemURL& url);

  // fileapi::FileSystemBackend overrides.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void Initialize(fileapi::FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const fileapi::FileSystemURL& url,
                          fileapi::OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(
          fileapi::FileSystemType type,
          base::File::Error* error_code) OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(
      const fileapi::FileSystemURL& url) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& path,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  // fileapi::ExternalFileSystemBackend overrides.
  virtual bool IsAccessAllowed(const fileapi::FileSystemURL& url)
      const OVERRIDE;
  virtual std::vector<base::FilePath> GetRootDirectories() const OVERRIDE;
  virtual void GrantFullAccessToExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id,
      const base::FilePath& virtual_path) OVERRIDE;
  virtual void RevokeAccessForExtension(
      const std::string& extension_id) OVERRIDE;
  virtual bool GetVirtualPath(const base::FilePath& filesystem_path,
                              base::FilePath* virtual_path) OVERRIDE;

 private:
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<fileapi::AsyncFileUtil> local_file_util_;

  // The delegate instance for the drive file system related operations.
  scoped_ptr<FileSystemBackendDelegate> drive_delegate_;

  // The delegate instance for the provided file system related operations.
  scoped_ptr<FileSystemBackendDelegate> file_system_provider_delegate_;

  // The delegate instance for the MTP file system related operations.
  scoped_ptr<FileSystemBackendDelegate> mtp_delegate_;

  // Mount points specific to the owning context (i.e. per-profile mount
  // points).
  //
  // It is legal to have mount points with the same name as in
  // system_mount_points_. Also, mount point paths may overlap with mount point
  // paths in system_mount_points_. In both cases mount points in
  // |mount_points_| will have a priority.
  // E.g. if |mount_points_| map 'foo1' to '/foo/foo1' and
  // |file_system_mount_points_| map 'xxx' to '/foo/foo1/xxx', |GetVirtualPaths|
  // will resolve '/foo/foo1/xxx/yyy' as 'foo1/xxx/yyy' (i.e. the mapping from
  // |mount_points_| will be used).
  scoped_refptr<fileapi::ExternalMountPoints> mount_points_;

  // Globally visible mount points. System MountPonts instance should outlive
  // all FileSystemBackend instances, so raw pointer is safe.
  fileapi::ExternalMountPoints* system_mount_points_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemBackend);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_H_
