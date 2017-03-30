// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_job_response_data.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"
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
  using CreateRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              std::vector<BackgroundFetchRequestInfo>)>;
  using DeleteRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using NextRequestCallback = base::OnceCallback<void(
      const base::Optional<BackgroundFetchRequestInfo>&)>;

  explicit BackgroundFetchDataManager(BrowserContext* browser_context);
  ~BackgroundFetchDataManager();

  // Creates and stores a new registration with the given properties. Will
  // invoke the |callback| when the registration has been created, which may
  // fail due to invalid input or storage errors.
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      CreateRegistrationCallback callback);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has been started as |download_guid|.
  void MarkRequestAsStarted(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchRequestInfo& request,
      const std::string& download_guid);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has completed. Will invoke the |callback| with the
  // next request, if any, when the operation has completed.
  void MarkRequestAsCompleteAndGetNextRequest(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchRequestInfo& request,
      NextRequestCallback callback);

  // Deletes the registration identified by |registration_id|. Will invoke the
  // |callback| when the registration has been deleted from storage.
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          DeleteRegistrationCallback callback);

  // TODO(harkness): Replace the OnceClosure with a callback to return the
  // response object once it is decided whether lifetime should be passed to the
  // caller or stay here.
  void GetJobResponse(const std::string& job_guid,
                      const BackgroundFetchResponseCompleteCallback& callback);

  // The following methods are called by the JobController to update job state.
  // Returns a boolean indicating whether there are more requests to process.
  bool UpdateRequestState(const std::string& job_guid,
                          const std::string& request_guid,
                          DownloadItem::DownloadState state,
                          DownloadInterruptReason interrupt_reason);
  void UpdateRequestStorageState(const std::string& job_guid,
                                 const std::string& request_guid,
                                 const base::FilePath& file_path,
                                 int64_t received_bytes);
  void UpdateRequestDownloadGuid(const std::string& job_guid,
                                 const std::string& request_guid,
                                 const std::string& download_guid);

  // Called by the JobController to get a BackgroundFetchRequestInfo to
  // process.
  const BackgroundFetchRequestInfo& GetNextBackgroundFetchRequestInfo(
      const std::string& job_guid);

  // Indicates whether all requests have been handed to the JobController.
  bool HasRequestsRemaining(const std::string& job_guid) const;

  // Indicates whether all requests have been handed out and completed.
  bool IsComplete(const std::string& job_guid) const;

 private:
  friend class BackgroundFetchDataManagerTest;

  class RegistrationData;

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

  // Map of known background fetch registration ids to their associated data.
  std::map<BackgroundFetchRegistrationId, std::unique_ptr<RegistrationData>>
      registrations_;

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
