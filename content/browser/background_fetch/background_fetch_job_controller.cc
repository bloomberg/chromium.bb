// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <utility>

#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDelegateProxy* delegate_proxy,
    BackgroundFetchScheduler* scheduler,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    uint64_t bytes_downloaded,
    ProgressCallback progress_callback,
    BackgroundFetchScheduler::FinishedCallback finished_callback)
    : BackgroundFetchScheduler::Controller(scheduler,
                                           registration_id,
                                           std::move(finished_callback)),
      options_(options),
      icon_(icon),
      complete_requests_downloaded_bytes_cache_(bytes_downloaded),
      delegate_proxy_(delegate_proxy),
      progress_callback_(std::move(progress_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchJobController::InitializeRequestStatus(
    int completed_downloads,
    int total_downloads,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    const std::string& ui_title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't allow double initialization.
  DCHECK_GT(total_downloads, 0);
  DCHECK_EQ(total_downloads_, 0);

  completed_downloads_ = completed_downloads;
  total_downloads_ = total_downloads;

  // TODO(nator): Update this when we support uploads.
  int total_downloads_size = options_.download_total;

  std::vector<std::string> active_guids;
  active_guids.reserve(active_fetch_requests.size());
  for (const auto& request_info : active_fetch_requests)
    active_guids.push_back(request_info->download_guid());

  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      registration_id().unique_id(), ui_title, registration_id().origin(),
      icon_, completed_downloads, total_downloads,
      complete_requests_downloaded_bytes_cache_, total_downloads_size,
      std::move(active_guids));

  delegate_proxy_->CreateDownloadJob(GetWeakPtr(), std::move(fetch_description),
                                     std::move(active_fetch_requests));
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool BackgroundFetchJobController::HasMoreRequests() {
  return completed_downloads_ < total_downloads_;
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    RequestFinishedCallback request_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LT(completed_downloads_, total_downloads_);
  DCHECK(request_finished_callback);
  DCHECK(request);

  active_request_downloaded_bytes_ = 0;
  active_request_finished_callback_ = std::move(request_finished_callback);

  delegate_proxy_->StartRequest(registration_id().unique_id(),
                                registration_id().origin(), request);
}

void BackgroundFetchJobController::DidStartRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(delphick): Either add CORS check here or remove this function and do
  // the CORS check in BackgroundFetchDelegateImpl (since
  // download::Client::OnDownloadStarted returns a value that can abort the
  // download).
}

void BackgroundFetchJobController::DidUpdateRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (active_request_downloaded_bytes_ == bytes_downloaded)
    return;

  active_request_downloaded_bytes_ = bytes_downloaded;

  progress_callback_.Run(registration_id().unique_id(), options_.download_total,
                         complete_requests_downloaded_bytes_cache_ +
                             GetInProgressDownloadedBytes());
}

void BackgroundFetchJobController::DidCompleteRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It's possible for the DidCompleteRequest() callback to have been in-flight
  // while this Job Controller was being aborted, in which case the
  // |active_request_finished_callback_| will have been reset.
  if (!active_request_finished_callback_)
    return;

  active_request_downloaded_bytes_ = 0;

  complete_requests_downloaded_bytes_cache_ += request->GetFileSize();
  ++completed_downloads_;

  std::move(active_request_finished_callback_).Run(request);
}

void BackgroundFetchJobController::UpdateUI(
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_->UpdateUI(registration_id().unique_id(), title, icon);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  return active_request_downloaded_bytes_;
}

void BackgroundFetchJobController::Abort(
    BackgroundFetchReasonToAbort reason_to_abort) {
  // Stop propagating any in-flight events to the scheduler.
  active_request_finished_callback_.Reset();

  // Cancel any in-flight downloads and UI through the BGFetchDelegate.
  delegate_proxy_->Abort(registration_id().unique_id());

  Finish(reason_to_abort);
}

}  // namespace content
