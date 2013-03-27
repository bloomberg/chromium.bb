// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client_interface.h"
#include "net/base/network_change_notifier.h"

class GURL;
class Profile;

namespace google_apis {
class DriveUploaderInterface;
}

namespace sync_file_system {

// This class is responsible for talking to the Drive service to get and put
// Drive directories, files and metadata.
// This class is owned by DriveFileSyncService.
class DriveFileSyncClient
    : public DriveFileSyncClientInterface,
      public google_apis::DriveServiceObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveFileSyncClient> {
 public:
  explicit DriveFileSyncClient(Profile* profile);
  virtual ~DriveFileSyncClient();

  virtual void AddObserver(DriveFileSyncClientObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveFileSyncClientObserver* observer) OVERRIDE;

  static scoped_ptr<DriveFileSyncClient> CreateForTesting(
      Profile* profile,
      const GURL& base_url,
      scoped_ptr<google_apis::DriveServiceInterface> drive_service,
      scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader);

  // DriveFileSyncClientInterface overrides.
  virtual void GetDriveDirectoryForSyncRoot(
      const ResourceIdCallback& callback) OVERRIDE;
  virtual void GetDriveDirectoryForOrigin(
      const std::string& sync_root_resource_id,
      const GURL& origin,
      const ResourceIdCallback& callback) OVERRIDE;
  virtual void GetLargestChangeStamp(
      const ChangeStampCallback& callback) OVERRIDE;
  virtual void GetResourceEntry(
      const std::string& resource_id,
      const ResourceEntryCallback& callback) OVERRIDE;
  virtual void ListFiles(
      const std::string& directory_resource_id,
      const ResourceListCallback& callback) OVERRIDE;
  virtual void ListChanges(
      int64 start_changestamp,
      const ResourceListCallback& callback) OVERRIDE;
  virtual void ContinueListing(
      const GURL& feed_url,
      const ResourceListCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const std::string& resource_id,
      const std::string& local_file_md5,
      const base::FilePath& local_file_path,
      const DownloadFileCallback& callback) OVERRIDE;
  virtual void UploadNewFile(
      const std::string& directory_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const UploadFileCallback& callback) OVERRIDE;
  virtual void UploadExistingFile(
      const std::string& resource_id,
      const std::string& remote_file_md5,
      const base::FilePath& local_file_path,
      const UploadFileCallback& callback) OVERRIDE;
  virtual bool IsAuthenticated() const OVERRIDE;
  virtual void DeleteFile(
      const std::string& resource_id,
      const std::string& remote_file_md5,
      const GDataErrorCallback& callback) OVERRIDE;
  virtual GURL ResourceIdToResourceLink(
      const std::string& resource_id) const OVERRIDE;
  virtual void EnsureSyncRootIsNotInMyDrive(
      const std::string& sync_root_resource_id) const OVERRIDE;

  static std::string OriginToDirectoryTitle(const GURL& origin);
  static GURL DirectoryTitleToOrigin(const std::string& title);

  // DriveServiceObserver overrides.
  virtual void OnReadyToPerformOperations() OVERRIDE;

  // ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  typedef int64 UploadKey;
  typedef std::map<UploadKey, UploadFileCallback> UploadCallbackMap;

  friend class DriveFileSyncClientTest;
  friend class DriveFileSyncServiceMockTest;

  // Constructor for test use.
  DriveFileSyncClient(
      Profile* profile,
      const GURL& base_url,
      scoped_ptr<google_apis::DriveServiceInterface> drive_service,
      scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader);

  void DidGetDirectory(const std::string& parent_resource_id,
                       const std::string& directory_name,
                       const ResourceIdCallback& callback,
                       google_apis::GDataErrorCode error,
                       scoped_ptr<google_apis::ResourceList> feed);

  void DidCreateDirectory(const std::string& parent_resource_id,
                          const std::string& title,
                          const ResourceIdCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<google_apis::ResourceEntry> entry);

  void DidEnsureUniquenessForCreateDirectory(
      const ResourceIdCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  void SearchFilesInDirectory(const std::string& directory_resource_id,
                              const std::string& search_query,
                              const ResourceListCallback& callback);

  void DidGetAboutResource(
      const ChangeStampCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);

  void DidGetResourceList(
      const ResourceListCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  void DidGetResourceEntry(const ResourceEntryCallback& callback,
                           google_apis::GDataErrorCode error,
                           scoped_ptr<google_apis::ResourceEntry> entry);

  void DownloadFileInternal(const std::string& local_file_md5,
                            const base::FilePath& local_file_path,
                            const DownloadFileCallback& callback,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceEntry> entry);

  void DidDownloadFile(const std::string& downloaded_file_md5,
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
      scoped_ptr<google_apis::ResourceEntry> entry);

  void UploadExistingFileInternal(
      const std::string& remote_file_md5,
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
                             const ResourceEntryCallback& callback);
  void DidListEntriesToEnsureUniqueness(
      const std::string& parent_resource_id,
      const std::string& expected_title,
      const ResourceEntryCallback& callback,
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

  static std::string FormatTitleQuery(const std::string& title);

  scoped_ptr<google_apis::DriveServiceInterface> drive_service_;
  scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader_;
  google_apis::GDataWapiUrlGenerator url_generator_;

  UploadCallbackMap upload_callback_map_;
  UploadKey upload_next_key_;

  ObserverList<DriveFileSyncClientObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClient);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
