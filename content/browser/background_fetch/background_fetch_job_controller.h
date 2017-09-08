// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class BackgroundFetchDataManager;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the DataManager and dispatch them
// to the DownloadManager. It lives entirely on the IO thread.
class CONTENT_EXPORT BackgroundFetchJobController
    : public BackgroundFetchDelegateProxy::Controller {
 public:
  enum class State { INITIALIZED, FETCHING, ABORTED, COMPLETED };

  using CompletedCallback =
      base::OnceCallback<void(BackgroundFetchJobController*)>;

  BackgroundFetchJobController(
      BackgroundFetchDelegateProxy* delegate_proxy,
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      BackgroundFetchDataManager* data_manager,
      CompletedCallback completed_callback);
  ~BackgroundFetchJobController();

  // Starts fetching the first few requests. The controller will continue to
  // fetch new content until all requests have been handled.
  void Start();

  // Updates the representation of this Background Fetch in the user interface
  // to match the given |title|.
  void UpdateUI(const std::string& title);

  // Immediately aborts this Background Fetch by request of the developer.
  void Abort();

  // Returns the current state of this Job Controller.
  State state() const { return state_; }

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
  void DidCompleteRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request) override;

 private:
  // Requests the download manager to start fetching |request|.
  void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request);

  // Called when a completed download has been marked as such in DataManager.
  void DidMarkRequestCompleted(bool has_pending_or_active_requests);

  // The registration id on behalf of which this controller is fetching data.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  BackgroundFetchOptions options_;

  // The current state of this Job Controller.
  State state_ = State::INITIALIZED;

  // The DataManager's lifetime is controlled by the BackgroundFetchContext and
  // will be kept alive until after the JobController is destroyed.
  BackgroundFetchDataManager* data_manager_;

  // Proxy for interacting with the BackgroundFetchDelegate across thread
  // boundaries. It is owned by the BackgroundFetchContext.
  BackgroundFetchDelegateProxy* delegate_proxy_;

  // Callback for when all fetches have been completed.
  CompletedCallback completed_callback_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
