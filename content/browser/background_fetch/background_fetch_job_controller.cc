// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDelegateProxy* delegate_proxy,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const BackgroundFetchRegistration& registration,
    int completed_downloads,
    int total_downloads,
    const std::vector<std::string>& outstanding_guids,
    BackgroundFetchDataManager* data_manager,
    ProgressCallback progress_callback,
    FinishedCallback finished_callback)
    : registration_id_(registration_id),
      options_(options),
      complete_requests_downloaded_bytes_cache_(registration.downloaded),
      data_manager_(data_manager),
      delegate_proxy_(delegate_proxy),
      progress_callback_(std::move(progress_callback)),
      finished_callback_(std::move(finished_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->SetController(registration_id, this);

  delegate_proxy_->CreateDownloadJob(
      registration_id.unique_id(), options.title, registration_id.origin(),
      GetWeakPtr(), completed_downloads, total_downloads, outstanding_guids);
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->SetController(registration_id_, nullptr);
}

void BackgroundFetchJobController::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/741609): Enforce kMaximumBackgroundFetchParallelRequests
  // globally and/or per origin rather than per fetch.
  for (size_t i = 0; i < kMaximumBackgroundFetchParallelRequests; i++) {
    data_manager_->PopNextRequest(
        registration_id_,
        base::BindOnce(&BackgroundFetchJobController::StartRequest,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!request) {
    // This can happen when |Start| tries to start multiple initial requests,
    // but the fetch does not contain that many pending requests; or when
    // |DidMarkRequestCompleted| tries to start the next request but there are
    // none left, perhaps because the registration was aborted.
    return;
  }

  delegate_proxy_->StartRequest(registration_id_.unique_id(),
                                registration_id_.origin(), request);
}

void BackgroundFetchJobController::DidStartRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->MarkRequestAsStarted(registration_id_, request.get(),
                                      download_guid);
}

void BackgroundFetchJobController::DidUpdateRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    const std::string& download_guid,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (active_request_download_bytes_[download_guid] == bytes_downloaded)
    return;

  active_request_download_bytes_[download_guid] = bytes_downloaded;

  progress_callback_.Run(registration_id_.unique_id(), options_.download_total,
                         complete_requests_downloaded_bytes_cache_ +
                             GetInProgressDownloadedBytes());
}

void BackgroundFetchJobController::DidCompleteRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This request is no longer in-progress, so the DataManager will take over
  // responsibility for storing its downloaded bytes, though still need a cache.
  active_request_download_bytes_.erase(download_guid);
  complete_requests_downloaded_bytes_cache_ += request->GetFileSize();

  // The DataManager must acknowledge that it stored the data and that there are
  // no more pending requests to avoid marking this job as completed too early.
  data_manager_->MarkRequestAsComplete(
      registration_id_, request.get(),
      base::BindOnce(&BackgroundFetchJobController::DidMarkRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchJobController::DidMarkRequestCompleted(
    bool has_pending_or_active_requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If not all requests have completed, start a pending request if there are
  // any left, and bail.
  if (has_pending_or_active_requests) {
    data_manager_->PopNextRequest(
        registration_id_,
        base::BindOnce(&BackgroundFetchJobController::StartRequest,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Otherwise the job this controller is responsible for has completed.
  if (finished_callback_) {
    // Copy registration_id_ onto stack as finished_callback_ may delete |this|.
    BackgroundFetchRegistrationId registration_id(registration_id_);

    // This call will be ignored if the job has already been aborted.
    std::move(finished_callback_).Run(registration_id, false /* aborted */);
  }
}

void BackgroundFetchJobController::AbortFromUser() {
  // Aborts from user come via the BackgroundFetchDelegate, which will have
  // already cancelled the download.
  Abort(false /* cancel_download */);
}

void BackgroundFetchJobController::UpdateUI(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_->UpdateUI(registration_id_.unique_id(), title);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  uint64_t sum = 0;
  for (const auto& entry : active_request_download_bytes_)
    sum += entry.second;
  return sum;
}

void BackgroundFetchJobController::Abort() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Abort(true /* cancel_download */);
}

void BackgroundFetchJobController::Abort(bool cancel_download) {
  if (cancel_download)
    delegate_proxy_->Abort(registration_id_.unique_id());

  if (!finished_callback_)
    return;

  // Copy registration_id_ onto stack as finished_callback_ may delete |this|.
  BackgroundFetchRegistrationId registration_id(registration_id_);

  // This call will be ignored if job has already completed/failed/aborted.
  std::move(finished_callback_).Run(registration_id, true /* aborted */);
}

}  // namespace content
