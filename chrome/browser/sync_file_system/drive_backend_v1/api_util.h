// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_API_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_API_UTIL_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util_interface.h"
#include "net/base/network_change_notifier.h"
#include "webkit/common/blob/scoped_file.h"

class GURL;
class Profile;
class ProfileOAuth2TokenService;
class SigninManagerBase;

namespace drive { class DriveUploaderInterface; }

namespace sync_file_system {
namespace drive_backend {

// This class is responsible for talking to the Drive service to get and put
// Drive directories, files and metadata.
// This class is owned by DriveFileSyncService.
class APIUtil : public APIUtilInterface,
                public drive::DriveServiceObserver,
                public net::NetworkChangeNotifier::ConnectionTypeObserver,
                public base::NonThreadSafe,
                public base::SupportsWeakPtr<APIUtil> {
 public:
  // The resulting status of EnsureTitleUniqueness.
  enum EnsureUniquenessStatus {
    NO_DUPLICATES_FOUND,
    RESOLVED_DUPLICATES,
  };

  typedef base::Callback<void(google_apis::GDataErrorCode,
                              EnsureUniquenessStatus status,
                              scoped_ptr<google_apis::ResourceEntry> entry)>
      EnsureUniquenessCallback;

  APIUtil(Profile* profile, const base::FilePath& temp_dir_path);
  virtual ~APIUtil();

  virtual void AddObserver(APIUtilObserver* observer) OVERRIDE;
  virtual void RemoveObserver(APIUtilObserver* observer) OVERRIDE;

  static scoped_ptr<APIUtil> CreateForTesting(
      const base::FilePath& temp_dir_path,
      scoped_ptr<drive::DriveServiceInterface> drive_service,
      scoped_ptr<drive::DriveUploaderInterface> drive_uploader);

  // APIUtilInterface overrides.
  virtual void GetDriveDirectoryForSyncRoot(const ResourceIdCallback& callback)
      OVERRIDE;
  virtual void GetDriveDirectoryForOrigin(
      const std::string& sync_root_resource_id,
      const GURL& origin,
      const ResourceIdCallback& callback) OVERRIDE;
  virtual void GetLargestChangeStamp(const ChangeStampCallback& callback)
      OVERRIDE;
  virtual void GetResourceEntry(const std::string& resource_id,
                                const ResourceEntryCallback& callback) OVERRIDE;
  virtual void ListFiles(const std::string& directory_resource_id,
                         const ResourceListCallback& callback) OVERRIDE;
  virtual void ListChanges(int64 start_changestamp,
                           const ResourceListCallback& callback) OVERRIDE;
  virtual void ContinueListing(const GURL& next_link,
                               const ResourceListCallback& callback) OVERRIDE;
  virtual void DownloadFile(const std::string& resource_id,
                            const std::string& local_file_md5,
                            const DownloadFileCallback& callback) OVERRIDE;
  virtual void UploadNewFile(const std::string& directory_resource_id,
                             const base::FilePath& local_file_path,
                             const std::string& title,
                             const UploadFileCallback& callback) OVERRIDE;
  virtual void UploadExistingFile(const std::string& resource_id,
                                  const std::string& remote_file_md5,
                                  const base::FilePath& local_file_path,
                                  const UploadFileCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const std::string& parent_resource_id,
                               const std::string& title,
                               const ResourceIdCallback& callback) OVERRIDE;
  virtual bool IsAuthenticated() const OVERRIDE;
  virtual void DeleteFile(const std::string& resource_id,
                          const std::string& remote_file_md5,
                          const GDataErrorCallback& callback) OVERRIDE;
  virtual void EnsureSyncRootIsNotInMyDrive(
      const std::string& sync_root_resource_id) OVERRIDE;

  static std::string GetSyncRootDirectoryName();
  static std::string OriginToDirectoryTitle(const GURL& origin);
  static GURL DirectoryTitleToOrigin(const std::string& title);

  // DriveServiceObserver overrides.
  virtual void OnReadyToSendRequests() OVERRIDE;

  // ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  typedef int64 UploadKey;
  typedef std::map<UploadKey, UploadFileCallback> UploadCallbackMap;

  friend class APIUtilTest;

  // Constructor for test use.
  APIUtil(const base::FilePath& temp_dir_path,
          scoped_ptr<drive::DriveServiceInterface> drive_service,
          scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
          const std::string& account_id);

  void GetDriveRootResourceId(const GDataErrorCallback& callback);
  void DidGetDriveRootResourceId(
      const GDataErrorCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);

  void DidGetDriveRootResourceIdForGetSyncRoot(
      const ResourceIdCallback& callback,
      google_apis::GDataErrorCode error);

  void DidGetDirectory(const std::string& parent_resource_id,
                       const std::string& directory_name,
                       const ResourceIdCallback& callback,
                       google_apis::GDataErrorCode error,
                       scoped_ptr<google_apis::ResourceList> feed);

  void DidCreateDirectory(const std::string& parent_resource_id,
                          const std::string& title,
                          const ResourceIdCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<google_apis::FileResource> entry);

  void DidEnsureUniquenessForCreateDirectory(
      const ResourceIdCallback& callback,
      google_apis::GDataErrorCode error,
      EnsureUniquenessStatus status,
      scoped_ptr<google_apis::ResourceEntry> entry);

  void SearchByTitle(const std::string& title,
                     const std::string& directory_resource_id,
                     const ResourceListCallback& callback);

  void DidGetLargestChangeStamp(
      const ChangeStampCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);

  void DidGetDriveRootResourceIdForEnsureSyncRoot(
      const std::string& sync_root_resource_id,
      google_apis::GDataErrorCode error);

  void DidGetFileList(const ResourceListCallback& callback,
                      google_apis::GDataErrorCode error,
                      scoped_ptr<google_apis::FileList> file_list);

  void DidGetChangeList(const ResourceListCallback& callback,
                        google_apis::GDataErrorCode error,
                        scoped_ptr<google_apis::ChangeList> change_list);

  void DidGetFileResource(const ResourceEntryCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<google_apis::FileResource> entry);

  void DidGetTemporaryFileForDownload(
      const std::string& resource_id,
      const std::string& local_file_md5,
      scoped_ptr<webkit_blob::ScopedFile> local_file,
      const DownloadFileCallback& callback,
      bool success);

  void DownloadFileInternal(const std::string& local_file_md5,
                            scoped_ptr<webkit_blob::ScopedFile> local_file,
                            const DownloadFileCallback& callback,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceEntry> entry);

  void DidDownloadFile(scoped_ptr<google_apis::ResourceEntry> entry,
                       scoped_ptr<webkit_blob::ScopedFile> local_file,
                       const DownloadFileCallback& callback,
                       google_apis::GDataErrorCode error,
                       const base::FilePath& downloaded_file_path);

  void DidUploadNewFile(const std::string& parent_resource_id,
                        const std::string& title,
                        UploadKey upload_key,
                        google_apis::GDataErrorCode error,
                        scoped_ptr<google_apis::ResourceEntry> entry);

  void DidEnsureUniquenessForCreateFile(
      const std::string& expected_resource_id,
      const UploadFileCallback& callback,
      google_apis::GDataErrorCode error,
      EnsureUniquenessStatus status,
      scoped_ptr<google_apis::ResourceEntry> entry);

  void UploadExistingFileInternal(const std::string& remote_file_md5,
                                  const base::FilePath& local_file_path,
                                  const UploadFileCallback& callback,
                                  google_apis::GDataErrorCode error,
                                  scoped_ptr<google_apis::ResourceEntry> entry);

  void DidUploadExistingFile(UploadKey upload_key,
                             google_apis::GDataErrorCode error,
                             scoped_ptr<google_apis::ResourceEntry> entry);

  void DeleteFileInternal(const std::string& remote_file_md5,
                          const GDataErrorCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<google_apis::ResourceEntry> entry);

  void DidDeleteFile(const GDataErrorCallback& callback,
                     google_apis::GDataErrorCode error);

  void EnsureTitleUniqueness(const std::string& parent_resource_id,
                             const std::string& expected_title,
                             const EnsureUniquenessCallback& callback);
  void DidListEntriesToEnsureUniqueness(
      const std::string& parent_resource_id,
      const std::string& expected_title,
      const EnsureUniquenessCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> feed);
  void DeleteEntriesForEnsuringTitleUniqueness(
      ScopedVector<google_apis::ResourceEntry> entries,
      const GDataErrorCallback& callback);
  void DidDeleteEntriesForEnsuringTitleUniqueness(
      ScopedVector<google_apis::ResourceEntry> entries,
      const GDataErrorCallback& callback,
      google_apis::GDataErrorCode error);

  UploadKey RegisterUploadCallback(const UploadFileCallback& callback);
  UploadFileCallback GetAndUnregisterUploadCallback(UploadKey key);
  void CancelAllUploads(google_apis::GDataErrorCode error);

  std::string GetRootResourceId() const;

  scoped_ptr<drive::DriveServiceInterface> drive_service_;
  scoped_ptr<drive::DriveUploaderInterface> drive_uploader_;

  ProfileOAuth2TokenService* oauth_service_;
  SigninManagerBase* signin_manager_;

  UploadCallbackMap upload_callback_map_;
  UploadKey upload_next_key_;

  base::FilePath temp_dir_path_;

  std::string root_resource_id_;

  bool has_initialized_token_;

  ObserverList<APIUtilObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(APIUtil);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_API_UTIL_H_
