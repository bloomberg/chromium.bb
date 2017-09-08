// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDelegateProxy* delegate_proxy,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    BackgroundFetchDataManager* data_manager,
    CompletedCallback completed_callback)
    : registration_id_(registration_id),
      options_(options),
      data_manager_(data_manager),
      delegate_proxy_(delegate_proxy),
      completed_callback_(std::move(completed_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  delegate_proxy_->StartRequest(weak_ptr_factory_.GetWeakPtr(),
                                registration_id_.origin(), request);
}

void BackgroundFetchJobController::DidStartRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->MarkRequestAsStarted(registration_id_, request.get(),
                                      download_guid);
}

void BackgroundFetchJobController::DidCompleteRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
