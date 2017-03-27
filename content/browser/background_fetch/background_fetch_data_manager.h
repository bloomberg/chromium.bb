// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_job_response_data.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class ChromeBlobStorageContext;

// The BackgroundFetchDataManager keeps track of all of the outstanding requests
// which are in process in the DownloadManager. When Chromium restarts, it is
// responsibile for reconnecting all the in progress downloads with an observer
// which will keep the metadata up to date.
class CONTENT_EXPORT BackgroundFetchDataManager {
 public:
  explicit BackgroundFetchDataManager(BrowserContext* browser_context);
  ~BackgroundFetchDataManager();

  // Called by BackgroundFetchContext when a new request is started, this will
  // store all of the necessary metadata to track the request.
  void CreateRequest(
      std::unique_ptr<BackgroundFetchJobInfo> job_info,
      std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos);

  // TODO(harkness): Replace the OnceClosure with a callback to return the
  // response object once it is decided whether lifetime should be passed to the
  // caller or stay here.
  void GetJobResponse(const std::string& job_guid,
                      const BackgroundFetchResponseCompleteCallback& callback);

  // The following methods are called by the JobController to update job state.
  // Returns a boolean indicating whether there are more requests to process.
  virtual bool UpdateRequestState(const std::string& job_guid,
                                  const std::string& request_guid,
                                  DownloadItem::DownloadState state,
                                  DownloadInterruptReason interrupt_reason);
  virtual void UpdateRequestStorageState(const std::string& job_guid,
                                         const std::string& request_guid,
                                         const base::FilePath& file_path,
                                         int64_t received_bytes);
  virtual void UpdateRequestDownloadGuid(const std::string& job_guid,
                                         const std::string& request_guid,
                                         const std::string& download_guid);

  // Called by the JobController to get a BackgroundFetchRequestInfo to
  // process.
  virtual const BackgroundFetchRequestInfo& GetNextBackgroundFetchRequestInfo(
      const std::string& job_guid);

  // Indicates whether all requests have been handed to the JobController.
  virtual bool HasRequestsRemaining(const std::string& job_guid) const;

  // Indicates whether all requests have been handed out and completed.
  virtual bool IsComplete(const std::string& job_guid) const;

 private:
  // Storage interface.
  void WriteJobToStorage(
      std::unique_ptr<BackgroundFetchJobInfo> job_info,
      std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos);
  void WriteRequestToStorage(const std::string& job_guid,
                             BackgroundFetchRequestInfo* request_info);
  std::unique_ptr<BackgroundFetchRequestInfo> GetRequestInfo(
      const std::string& job_guid,
      size_t request_index);

  void DidGetBlobStorageContext(
      const std::string& job_guid,
      const BackgroundFetchResponseCompleteCallback& callback,
      ChromeBlobStorageContext* blob_context);

  // Indirectly, this is owned by the BrowserContext.
  BrowserContext* browser_context_;

  // Set of known background fetch registration ids.
  std::set<BackgroundFetchRegistrationId> known_registrations_;

  // Map from job_guid to JobInfo.
  std::unordered_map<std::string, std::unique_ptr<BackgroundFetchJobInfo>>
      job_map_;

  // Temporary map to hold data which will be written to storage.
  // Map from job_guid to RequestInfos.
  std::unordered_map<std::string, std::vector<BackgroundFetchRequestInfo>>
      request_map_;

  base::WeakPtrFactory<BackgroundFetchDataManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
