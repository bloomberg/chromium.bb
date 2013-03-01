// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_INTERFACE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_INTERFACE_H_

#include <string>

#include "base/callback_forward.h"
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
  virtual void OnNetworkConnected() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClientObserver);
};

// The implementation of this class is responsible for talking to the Drive
// service to get and put Drive directories, files and metadata.
// This class is owned by DriveFileSyncService.
class DriveFileSyncClientInterface {
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
                              scoped_ptr<google_apis::ResourceList> feed)>
      ResourceListCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::ResourceEntry> entry)>
      ResourceEntryCallback;

  DriveFileSyncClientInterface() {}
  virtual ~DriveFileSyncClientInterface() {}

  virtual void AddObserver(DriveFileSyncClientObserver* observer) = 0;
  virtual void RemoveObserver(DriveFileSyncClientObserver* observer) = 0;

  // Fetches Resource ID of the directory where we should place all files to
  // sync.  Upon completion, invokes |callback|.
  // If the directory does not exist on the server this also creates
  // the directory.
  //
  // Returns HTTP_SUCCESS if the directory already exists.
  // Returns HTTP_CREATED if the directory was not found and created.
  virtual void GetDriveDirectoryForSyncRoot(
      const ResourceIdCallback& callback) = 0;

  // Fetches Resource ID of the directory for the |origin|.
  // Upon completion, invokes |callback|.
  // If the directory does not exist on the server this also creates
  // the directory.
  //
  // Returns HTTP_SUCCESS if the directory already exists.
  // Returns HTTP_CREATED if the directory was not found and created.
  virtual void GetDriveDirectoryForOrigin(
      const std::string& sync_root_resource_id,
      const GURL& origin,
      const ResourceIdCallback& callback) = 0;

  // Fetches the largest changestamp for the signed-in account.
  // Upon completion, invokes |callback|.
  virtual void GetLargestChangeStamp(const ChangeStampCallback& callback) = 0;

  // Fetches the resource entry for the file identified by |resource_id|.
  // Upon completion, invokes |callback|.
  virtual void GetResourceEntry(const std::string& resource_id,
                                const ResourceEntryCallback& callback) = 0;

  // Lists files in the directory identified by |resource_id|.
  // Upon completion, invokes |callback|.
  // The result may be chunked and may have successive results. The caller needs
  // to call ContunueListing with the result of GetNextFeedURL to get complete
  // list of files.
  virtual void ListFiles(const std::string& directory_resource_id,
                         const ResourceListCallback& callback) = 0;

  // Lists changes that happened after |start_changestamp|.
  // Upon completion, invokes |callback|.
  // The result may be chunked and may have successive results. The caller needs
  // to call ContunueListing with the result of GetNextFeedURL to get complete
  // list of changes.
  virtual void ListChanges(int64 start_changestamp,
                           const ResourceListCallback& callback) = 0;

  // Fetches the next chunk of ResourceList identified by |feed_url|.
  // Upon completion, invokes |callback|.
  virtual void ContinueListing(const GURL& feed_url,
                               const ResourceListCallback& callback) = 0;

  // Downloads the file identified by |resource_id| from Drive to
  // |local_file_path|.
  // |local_file_md5| represents the hash value of the local file to be updated.
  // If |local_file_md5| is equal to remote file's value, cancels the download
  // and invokes |callback| with GDataErrorCode::HTTP_NOT_MODIFIED immediately.
  // When there is no local file to be updated, |local_file_md5| should be
  // empty.
  virtual void DownloadFile(const std::string& resource_id,
                            const std::string& local_file_md5,
                            const base::FilePath& local_file_path,
                            const DownloadFileCallback& callback) = 0;

  // Uploads the new file |local_file_path| with specified |title| into the
  // directory identified by |directory_resource_id|.
  // Upon completion, invokes |callback| and returns HTTP_CREATED if the file
  // is created.
  virtual void UploadNewFile(const std::string& directory_resource_id,
                             const base::FilePath& local_file_path,
                             const std::string& title,
                             const UploadFileCallback& callback) = 0;

  // Uploads the existing file identified by |local_file_path|.
  // |remote_file_md5| represents the expected hash value of the file to be
  // updated on Drive. If |remote_file_md5| is different from the actual value,
  // cancels the upload and invokes |callback| with
  // GDataErrorCode::HTTP_CONFLICT immediately.
  // Returns HTTP_SUCCESS if the file uploaded successfully.
  virtual void UploadExistingFile(const std::string& resource_id,
                                  const std::string& remote_file_md5,
                                  const base::FilePath& local_file_path,
                                  const UploadFileCallback& callback) = 0;

  // Returns true if the user is authenticated.
  virtual bool IsAuthenticated() const = 0;

  // Deletes the file identified by |resource_id|.
  // A directory is considered a file and will cause a recursive delete if
  // given as the |resource_id|.
  //
  // |remote_file_md5| represents the expected hash value of the file to be
  // deleted from Drive. If |remote_file_md5| is empty, then it's implied that
  // the file should be deleted on the remote side regardless. If
  // |remote_file_md5| is not empty and is different from the actual value,
  // the deletion operation is canceled and the |callback| with
  // GDataErrorCode::HTTP_CONFLICT is invoked immediately.
  virtual void DeleteFile(const std::string& resource_id,
                          const std::string& remote_file_md5,
                          const GDataErrorCallback& callback) = 0;
  // Converts |resource_id| to corresponing resource link.
  virtual GURL ResourceIdToResourceLink(
      const std::string& resource_id) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClientInterface);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_INTERFACE_H_
