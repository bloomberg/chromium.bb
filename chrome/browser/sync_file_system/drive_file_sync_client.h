// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

class GURL;
class Profile;

namespace google_apis {
class DriveUploaderInterface;
}

namespace sync_file_system {

// This class is responsible for talking to the Drive service to get and put
// Drive directories, files and metadata.
// This class is owned by DriveFileSyncService.
class DriveFileSyncClient : public base::NonThreadSafe,
                            public base::SupportsWeakPtr<DriveFileSyncClient> {
 public:
  // TODO(tzik): Implement a function to map GDataErrorcode to SyncStatusCode.
  // crbug.com/157837
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              const std::string& resource_id)>
      ResourceIdCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              int64 changestamp)> ChangeStampCallback;
  typedef base::Callback<void(google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::DocumentFeed> feed)>
      DocumentFeedCallback;

  explicit DriveFileSyncClient(Profile* profile);
  virtual ~DriveFileSyncClient();

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

 private:
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

  void DidGetDocumentFeedData(const DocumentFeedCallback& callback,
                              google_apis::GDataErrorCode error,
                              scoped_ptr<base::Value> data);

  scoped_ptr<google_apis::DriveServiceInterface> drive_service_;
  scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClient);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_CLIENT_H_
