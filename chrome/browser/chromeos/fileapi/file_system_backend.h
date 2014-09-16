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
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/fileapi/file_system_types.h"

namespace storage {
class CopyOrMoveFileValidatorFactory;
class ExternalMountPoints;
class FileSystemURL;
class WatcherManager;
}  // namespace storage

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
class FileSystemBackend : public storage::ExternalFileSystemBackend {
 public:
  using storage::FileSystemBackend::OpenFileSystemCallback;

  // FileSystemBackend will take an ownership of a |mount_points|
  // reference. On the other hand, |system_mount_points| will be kept as a raw
  // pointer and it should outlive FileSystemBackend instance.
  // The ownerships of |drive_delegate| and |file_system_provider_delegate| are
  // also taken.
  FileSystemBackend(
      FileSystemBackendDelegate* drive_delegate,
      FileSystemBackendDelegate* file_system_provider_delegate,
      FileSystemBackendDelegate* mtp_delegate,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::ExternalMountPoints> mount_points,
      storage::ExternalMountPoints* system_mount_points);
  virtual ~FileSystemBackend();

  // Adds system mount points, such as "archive", and "removable". This
  // function is no-op if these mount points are already present.
  void AddSystemMountPoints();

  // Returns true if CrosMountpointProvider can handle |url|, i.e. its
  // file system type matches with what this provider supports.
  // This could be called on any threads.
  static bool CanHandleURL(const storage::FileSystemURL& url);

  // storage::FileSystemBackend overrides.
  virtual bool CanHandleType(storage::FileSystemType type) const OVERRIDE;
  virtual void Initialize(storage::FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const storage::FileSystemURL& url,
                          storage::OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) OVERRIDE;
  virtual storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) OVERRIDE;
  virtual storage::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(storage::FileSystemType type,
                                        base::File::Error* error_code) OVERRIDE;
  virtual storage::FileSystemOperation* CreateFileSystemOperation(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(
      const storage::FileSystemURL& url) const OVERRIDE;
  virtual bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const OVERRIDE;
  virtual scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& path,
      int64 offset,
      int64 max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) const OVERRIDE;
  virtual storage::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;
  virtual const storage::UpdateObserverList* GetUpdateObservers(
      storage::FileSystemType type) const OVERRIDE;
  virtual const storage::ChangeObserverList* GetChangeObservers(
      storage::FileSystemType type) const OVERRIDE;
  virtual const storage::AccessObserverList* GetAccessObservers(
      storage::FileSystemType type) const OVERRIDE;

  // storage::ExternalFileSystemBackend overrides.
  virtual bool IsAccessAllowed(
      const storage::FileSystemURL& url) const OVERRIDE;
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
  virtual void GetRedirectURLForContents(
      const storage::FileSystemURL& url,
      const storage::URLCallback& callback) OVERRIDE;

 private:
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<storage::AsyncFileUtil> local_file_util_;

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
  scoped_refptr<storage::ExternalMountPoints> mount_points_;

  // Globally visible mount points. System MountPonts instance should outlive
  // all FileSystemBackend instances, so raw pointer is safe.
  storage::ExternalMountPoints* system_mount_points_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemBackend);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_H_
