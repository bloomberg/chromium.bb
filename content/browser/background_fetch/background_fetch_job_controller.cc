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
    BackgroundFetchDataManager* data_manager,
    ProgressCallback progress_callback,
    CompletedCallback completed_callback)
    : registration_id_(registration_id),
      options_(options),
      complete_requests_downloaded_bytes_cache_(registration.downloaded),
      data_manager_(data_manager),
      delegate_proxy_(delegate_proxy),
      progress_callback_(std::move(progress_callback)),
      completed_callback_(std::move(completed_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->SetController(registration_id, this);
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->SetController(registration_id_, nullptr);
}

void BackgroundFetchJobController::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(state_, State::INITIALIZED);

  state_ = State::FETCHING;

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
  DCHECK_EQ(state_, State::FETCHING);
  if (!request) {
    // This can happen when |Start| tries to start multiple initial requests,
    // but the fetch does not contain that many pending requests; or when
    // |DidMarkRequestCompleted| tries to start the next request but there are
    // none left.
    return;
  }

  delegate_proxy_->StartRequest(registration_id_.unique_id(),
                                weak_ptr_factory_.GetWeakPtr(),
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
  DCHECK(state_ == State::FETCHING || state_ == State::ABORTED);

  // TODO(delphick): When ABORT is implemented correctly we should hopefully
  // never get here and the DCHECK above should only allow FETCHING.
  if (state_ == State::ABORTED)
    return;

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
  DCHECK_EQ(state_, State::FETCHING);

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
  state_ = State::COMPLETED;
  std::move(completed_callback_).Run(this);
}

void BackgroundFetchJobController::UpdateUI(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_->UpdateUI(title);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  uint64_t sum = 0;
  for (const auto& entry : active_request_download_bytes_)
    sum += entry.second;
  return sum;
}

void BackgroundFetchJobController::Abort() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  switch (state_) {
    case State::INITIALIZED:
    case State::FETCHING:
      break;
    case State::ABORTED:
    case State::COMPLETED:
      return;  // Ignore attempt to abort after completion/abort.
  }

  delegate_proxy_->Abort();

  state_ = State::ABORTED;
  // Inform the owner of the controller about the job having aborted.
  std::move(completed_callback_).Run(this);
}

}  // namespace content
