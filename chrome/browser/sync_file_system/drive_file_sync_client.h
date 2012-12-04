// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_upload_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

class GURL;
class Profile;

namespace google_apis {
class DriveUploaderInterface;
}

namespace sync_file_system {

class DriveFileSyncClientObserver {
 public:
  DriveFileSyncClientObserver() {}
  virtual ~DriveFileSyncClientObserver() {}
  virtual void OnAuthenticated() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClientObserver);
};

// This class is responsible for talking to the Drive service to get and put
// Drive directories, files and metadata.
// This class is owned by DriveFileSyncService.
class DriveFileSyncClient : public google_apis::DriveServiceObserver,
                            public base::NonThreadSafe,
                            public base::SupportsWeakPtr<DriveFileSyncClient> {
 public:
  typedef base::Callback<void(google_apis::GDataErrorCode error)>
      GDataErrorCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              const std::string& file_md5)>
      DownloadFileCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              const std::string& resource_id,
                              const std::string& file_md5)>
      UploadFileCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              const std::string& resource_id)>
      ResourceIdCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              int64 changestamp)> ChangeStampCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::DocumentFeed> feed)>
      DocumentFeedCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::DocumentEntry> entry)>
      DocumentEntryCallback;

  explicit DriveFileSyncClient(Profile* profile);
  virtual ~DriveFileSyncClient();

  void AddObserver(DriveFileSyncClientObserver* observer);
  void RemoveObserver(DriveFileSyncClientObserver* observer);

  static scoped_ptr<DriveFileSyncClient> CreateForTesting(
      Profile* profile,
      scoped_ptr<google_apis::DriveServiceInterface> drive_service,
      scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader);

  // Fetches Resource ID of the directory where we should place all files to
  // sync.  Upon completion, invokes |callback|.
  // If the directory does not exist on the server this also creates
  // the directory.
  void GetDriveDirectoryForSyncRoot(const ResourceIdCallback& callback);

  // Fetches Resource ID of the directory for the |origin|.
  // Upon completion, invokes |callback|.
  // If the directory does not exist on the server this also creates
  // the directory.
  void GetDriveDirectoryForOrigin(const std::string& sync_root_resource_id,
                                  const GURL& origin,
                                  const ResourceIdCallback& callback);

  // Fetches the largest changestamp for the signed-in account.
  // Upon completion, invokes |callback|.
  void GetLargestChangeStamp(const ChangeStampCallback& callback);

  // Fetches the document entry for the file identified by |resource_id|.
  // Upon completion, invokes |callback|.
  void GetDocumentEntry(const std::string& resource_id,
                        const DocumentEntryCallback& callback);

  // Lists files in the directory identified by |resource_id|.
  // Upon completion, invokes |callback|.
  // The result may be chunked and may have successive results. The caller needs
  // to call ContunueListing with the result of GetNextFeedURL to get complete
  // list of files.
  void ListFiles(const std::string& directory_resource_id,
                 const DocumentFeedCallback& callback);

  // Lists changes that happened after |start_changestamp|.
  // Upon completion, invokes |callback|.
  // The result may be chunked and may have successive results. The caller needs
  // to call ContunueListing with the result of GetNextFeedURL to get complete
  // list of changes.
  void ListChanges(int64 start_changestamp,
                   const DocumentFeedCallback& callback);

  // Fetches the next chunk of DocumentFeed identified by |feed_url|.
  // Upon completion, invokes |callback|.
  void ContinueListing(const GURL& feed_url,
                       const DocumentFeedCallback& callback);

  // Downloads the file identified by |resource_id| from Drive to
  // |local_file_path|.
  // |local_file_md5| represents the hash value of the local file to be updated.
  // If |local_file_md5| is equal to remote file's value, cancels the download
  // and invokes |callback| with GDataErrorCode::HTTP_NOT_MODIFIED immediately.
  // When there is no local file to be updated, |local_file_md5| should be
  // empty.
  void DownloadFile(const std::string& resource_id,
                    const std::string& local_file_md5,
                    const FilePath& local_file_path,
                    const DownloadFileCallback& callback);

  // Uploads the new file |local_file_path| with specified |title| into the
  // directory identified by |directory_resource_id|.
  // Upon completion, invokes |callback|.
  void UploadNewFile(const std::string& directory_resource_id,
                     const FilePath& local_file_path,
                     const std::string& title,
                     int64 file_size,
                     const UploadFileCallback& callback);

  // Uploads the existing file identified by |local_file_path|.
  // |remote_file_md5| represents the expected hash value of the file to be
  // updated on Drive. If |remote_file_md5| is different from the actual value,
  // cancels the upload and invokes |callback| with
  // GDataErrorCode::HTTP_CONFLICT immediately.
  void UploadExistingFile(const std::string& resource_id,
                          const std::string& remote_file_md5,
                          const FilePath& local_file_path,
                          int64 file_size,
                          const UploadFileCallback& callback);

  // Returns true if the user is authenticated.
  bool IsAuthenticated() const {
    return drive_service_->HasRefreshToken();
  }

  // Deletes the file identified by |resource_id|.
  // |remote_file_md5| represents the expected hash value of the file to be
  // deleted from Drive. If |remote_file_md5| is different from the actual
  // value, cancels the deletion and invokes |callback| with
  // GDataErrorCode::HTTP_CONFLICT immediately.
  void DeleteFile(const std::string& resource_id,
                  const std::string& remote_file_md5,
                  const GDataErrorCallback& callback);

  static std::string OriginToDirectoryTitle(const GURL& origin);
  static GURL DirectoryTitleToOrigin(const std::string& title);

  // DriveServiceObserver overrides.
  virtual void OnReadyToPerformOperations() OVERRIDE;

 private:
  friend class DriveFileSyncClientTest;
  friend class DriveFileSyncServiceTest;

  // Constructor for test use.
  DriveFileSyncClient(
      Profile* profile,
      scoped_ptr<google_apis::DriveServiceInterface> drive_service,
      scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader);

  void DidGetDirectory(const std::string& parent_resource_id,
                       const std::string& directory_name,
                       const ResourceIdCallback& callback,
                       google_apis::GDataErrorCode error,
                       scoped_ptr<google_apis::DocumentFeed> feed);

  void DidGetParentDirectoryForCreateDirectory(
      const FilePath::StringType& directory_name,
      const ResourceIdCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<base::Value> data);

  void DidCreateDirectory(const ResourceIdCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<base::Value> data);

  void SearchFilesInDirectory(const std::string& directory_resource_id,
                              const std::string& search_query,
                              const DocumentFeedCallback& callback);

  void DidGetAccountMetadata(const ChangeStampCallback& callback,
                             google_apis::GDataErrorCode error,
                             scoped_ptr<base::Value> data);

  void DidGetDocumentFeedData(const DocumentFeedCallback& callback,
                              google_apis::GDataErrorCode error,
                              scoped_ptr<base::Value> data);

  void DidGetDocumentEntryData(const DocumentEntryCallback& callback,
                               google_apis::GDataErrorCode error,
                               scoped_ptr<base::Value> data);

  void DownloadFileInternal(const std::string& local_file_md5,
                            const FilePath& local_file_path,
                            const DownloadFileCallback& callback,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::DocumentEntry> entry);

  void DidDownloadFile(const std::string& downloaded_file_md5,
                       const DownloadFileCallback& callback,
                       google_apis::GDataErrorCode error,
                       const FilePath& downloaded_file_path);

  void UploadNewFileInternal(
      const FilePath& local_file_path,
      const std::string& title,
      int64 file_size,
      const UploadFileCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::DocumentEntry> parent_directory_entry);

  void UploadExistingFileInternal(
      const std::string& remote_file_md5,
      const FilePath& local_file_path,
      int64 file_size,
      const UploadFileCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::DocumentEntry> entry);

  void DidUploadFile(const UploadFileCallback& callback,
                     google_apis::DriveUploadError error,
                     const FilePath& drive_path,
                     const FilePath& file_path,
                     scoped_ptr<google_apis::DocumentEntry> entry);

  void DeleteFileInternal(const std::string& remote_file_md5,
                          const GDataErrorCallback& callback,
                          google_apis::GDataErrorCode error,
                          scoped_ptr<google_apis::DocumentEntry> entry);

  void DidDeleteFile(const GDataErrorCallback& callback,
                     google_apis::GDataErrorCode error);

  static std::string FormatTitleQuery(const std::string& title);

  scoped_ptr<google_apis::DriveServiceInterface> drive_service_;
  scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader_;

  ObserverList<DriveFileSyncClientObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClient);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
