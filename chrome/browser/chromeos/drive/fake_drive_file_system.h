// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_DRIVE_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_DRIVE_FILE_SYSTEM_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace google_apis {

class AboutResource;
class DriveServiceInterface;
class ResourceEntry;

}  // namespace google_apis

namespace drive {

class DriveEntryProto;
class DriveFileSystemObserver;

namespace test_util {

// This class implements a fake DriveFileSystem which acts like a real Drive
// file system with FakeDriveService, for testing purpose.
// Note that this class doesn't support "caching" at the moment, so the number
// of interactions to the FakeDriveService may be bigger than the real
// implementation.
// Currently most methods are empty (not implemented).
class FakeDriveFileSystem : public DriveFileSystemInterface {
 public:

  explicit FakeDriveFileSystem(
      google_apis::DriveServiceInterface* drive_service);
  virtual ~FakeDriveFileSystem();

  // DriveFileSystemInterface Overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(DriveFileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveFileSystemObserver* observer) OVERRIDE;
  virtual void StartInitialFeedFetch() OVERRIDE;
  virtual void StartPolling() OVERRIDE;
  virtual void StopPolling() OVERRIDE;
  virtual void SetPushNotificationEnabled(bool enabled) OVERRIDE;
  virtual void NotifyFileSystemMounted() OVERRIDE;
  virtual void NotifyFileSystemToBeUnmounted() OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) OVERRIDE;
  virtual void TransferFileFromRemoteToLocal(
      const base::FilePath& remote_src_file_path,
      const base::FilePath& local_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void CloseFile(const base::FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const base::FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CancelGetFile(const base::FilePath& drive_file_path) OVERRIDE;
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetEntryInfoByPath(
      const base::FilePath& file_path,
      const GetEntryInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const base::FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback) OVERRIDE;
  virtual void RefreshDirectory(
      const base::FilePath& file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      bool shared_with_me,
                      const GURL& next_feed,
                      const SearchCallback& callback) OVERRIDE;
  virtual void SearchMetadata(const std::string& query,
                              int at_most_num_matches,
                              const SearchMetadataCallback& callback) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void AddUploadedFile(scoped_ptr<google_apis::ResourceEntry> doc_entry,
                               const base::FilePath& file_content_path,
                               const FileOperationCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) OVERRIDE;
  virtual void Reload() OVERRIDE;

 private:

  // Callback to return the result of GetFilePath.
  typedef base::Callback<void(const base::FilePath& file_path)>
      GetFilePathCallback;

  // Returns the path for the |resource_id| via |callback|.
  // How the method works:
  // 1) Gets AboutResource from the drive service to obtain root resource id.
  // 2) Gets ResourceEntry from the drive service to get the base name,
  //    prepends it to the |file_path|. Unless it is root, also tries for
  //    the parent recursively.
  void GetFilePath(const std::string& resource_id,
                   const GetFilePathCallback& callback);
  void GetFilePathAfterGetAboutResource(
      const std::string& resource_id,
      const GetFilePathCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);
  void GetFilePathInternal(
      const std::string& root_resource_id,
      const std::string& resource_id,
      const base::FilePath& file_path,
      const GetFilePathCallback& callback);
  void GetFilePathAfterGetResourceEntry(
      const std::string& root_resource_id,
      const base::FilePath& remaining_file_path,
      const GetFilePathCallback& callback,
      google_apis::GDataErrorCode error_in,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Helpers of GetEntryInfoByResourceId.
  // How the method works:
  // 1) Gets ResourceEntry from the drive service.
  // 2) Gets the file path of the resource.
  // 3) Runs the |callback|.
  void GetEntryInfoByResourceIdAfterGetResourceEntry(
      const GetEntryInfoWithFilePathCallback& callback,
      google_apis::GDataErrorCode error_in,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);
  void GetEntryInfoByResourceIdAfterGetFilePath(
      const GetEntryInfoWithFilePathCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto,
      const base::FilePath& parent_file_path);

  google_apis::DriveServiceInterface* drive_service_;  // Not owned.

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeDriveFileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveFileSystem);
};

}  // namespace test_util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_DRIVE_FILE_SYSTEM_H_
