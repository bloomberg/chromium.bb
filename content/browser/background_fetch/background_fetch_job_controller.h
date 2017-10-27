// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the DataManager and dispatch them
// to the DownloadManager. It lives entirely on the IO thread.
//
// Lifetime: It is created lazily only once a Background Fetch registration
// starts downloading, and it is destroyed once no more communication with the
// DownloadManager or Offline Items Collection is necessary (i.e. once the
// registration has been aborted, or once it has completed/failed and the
// waitUntil promise has been resolved so UpdateUI can no longer be called).
class CONTENT_EXPORT BackgroundFetchJobController final
    : public BackgroundFetchDelegateProxy::Controller,
      public BackgroundFetchDataManager::Controller {
 public:
  using FinishedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              bool /* aborted */)>;
  using ProgressCallback =
      base::RepeatingCallback<void(const std::string& /* unique_id */,
                                   uint64_t /* download_total */,
                                   uint64_t /* downloaded */)>;
  BackgroundFetchJobController(
      BackgroundFetchDelegateProxy* delegate_proxy,
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      const BackgroundFetchRegistration& registration,
      int completed_downloads,
      int total_downloads,
      const std::vector<std::string>& outstanding_guids,
      BackgroundFetchDataManager* data_manager,
      ProgressCallback progress_callback,
      FinishedCallback finished_callback);
  ~BackgroundFetchJobController() override;

  // Starts fetching the first few requests. The controller will continue to
  // fetch new content until all requests have been handled.
  void Start();

  // BackgroundFetchDataManager::Controller implementation:
  void UpdateUI(const std::string& title) override;
  uint64_t GetInProgressDownloadedBytes() override;
  void Abort() override;

  // Returns the registration id for which this job is fetching data.
  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  // Returns the options with which this job is fetching data.
  const BackgroundFetchOptions& options() const { return options_; }

  base::WeakPtr<BackgroundFetchJobController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // BackgroundFetchDelegateProxy::Controller implementation:
  void DidStartRequest(const scoped_refptr<BackgroundFetchRequestInfo>& request,
                       const std::string& download_guid) override;
  void DidUpdateRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request,
      const std::string& download_guid,
      uint64_t bytes_downloaded) override;
  void DidCompleteRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request,
      const std::string& download_guid) override;
  void AbortFromUser() override;

 private:
  // Aborts a job updating the registration with the new state. If
  // |cancel_download| is true, the ongoing download is also cancelled
  // (otherwise it assumes that has already happened).
  void Abort(bool cancel_download);

  // Requests the download manager to start fetching |request|.
  void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request);

  // Called when a completed download has been marked as such in DataManager.
  void DidMarkRequestCompleted(bool has_pending_or_active_requests);

  // The registration id on behalf of which this controller is fetching data.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  BackgroundFetchOptions options_;

  // Map from in-progress |download_guid|s to number of bytes downloaded.
  base::flat_map<std::string, uint64_t> active_request_download_bytes_;

  // Cache of downloaded byte count stored by the DataManager, to enable
  // delivering progress events without having to read from the database.
  uint64_t complete_requests_downloaded_bytes_cache_;

  // The DataManager's lifetime is controlled by the BackgroundFetchContext and
  // will be kept alive until after the JobController is destroyed.
  BackgroundFetchDataManager* data_manager_;

  // Proxy for interacting with the BackgroundFetchDelegate across thread
  // boundaries. It is owned by the BackgroundFetchContext.
  BackgroundFetchDelegateProxy* delegate_proxy_;

  // Callback run each time download progress updates.
  ProgressCallback progress_callback_;

  // Callback for when all fetches have completed/failed/aborted.
  FinishedCallback finished_callback_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
