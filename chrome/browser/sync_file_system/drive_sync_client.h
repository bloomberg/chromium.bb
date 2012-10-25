// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_SYNC_CLIENT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_SYNC_CLIENT_H_

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
class DriveServiceInterface;
class DriveUploader;
}

namespace sync_file_system {

// This class is responsible for talking to the Drive service to get and put
// Drive folders, files and metadata.
// This class is owned by DriveFileSyncService.
class DriveSyncClient : public google_apis::DriveServiceObserver,
                        public base::NonThreadSafe,
                        public base::SupportsWeakPtr<DriveSyncClient> {
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

  explicit DriveSyncClient(Profile* profile);
  virtual ~DriveSyncClient();

  // DriveServiceObserver overrides.
  virtual void OnReadyToPerformOperations() OVERRIDE;
  virtual void OnAuthenticationFailed(
      google_apis::GDataErrorCode error) OVERRIDE;

  // Fetches Resource ID of the directory where we should place all files to
  // sync.  Upon completion, invokes |callback|.
  // If the folder does not exist on the server this also creates the folder.
  void GetDriveFolderForSyncData(const ResourceIdCallback& callback);

  // Fetches Resource ID of the directory for the |origin|.
  // Upon completion, invokes |callback|.
  // If the folder does not exist on the server this also creates the folder.
  void GetDriveFolderForOrigin(const GURL& origin,
                               const ResourceIdCallback& callback);

  // Fetches the largest changestamp for the signed-in account.
  // Upon completion, invokes |callback|.
  void GetLargestChangeStamp(const ChangeStampCallback& callback);

  // Lists files in the directory identified by |resource_id|.
  // Upon completion, invokes |callback|.
  // The result may be chunked and may have successive results. The caller needs
  // to call ContunueListing with the result of GetNextFeedURL to get complete
  // list of files.
  void ListFiles(std::string resource_id, const DocumentFeedCallback& callback);

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
  scoped_ptr<google_apis::DriveServiceInterface> drive_service_;
  scoped_ptr<google_apis::DriveUploader> drive_uploader_;

  DISALLOW_COPY_AND_ASSIGN(DriveSyncClient);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_SYNC_CLIENT_H_
