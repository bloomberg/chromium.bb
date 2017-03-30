// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    BackgroundFetchDataManager* data_manager,
    CompletedCallback completed_callback)
    : registration_id_(registration_id),
      options_(options),
      browser_context_(browser_context),
      storage_partition_(storage_partition),
      data_manager_(data_manager),
      completed_callback_(std::move(completed_callback)),
      weak_ptr_factory_(this) {}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  // TODO(harkness): Write final status to the DataManager.

  for (const auto& pair : downloads_)
    pair.first->RemoveObserver(this);
}

void BackgroundFetchJobController::Start(
    std::vector<BackgroundFetchRequestInfo> initial_requests) {
  DCHECK_LE(initial_requests.size(), kMaximumBackgroundFetchParallelRequests);
  DCHECK_EQ(state_, State::INITIALIZED);

  state_ = State::FETCHING;

  for (const BackgroundFetchRequestInfo& request : initial_requests)
    StartRequest(request);
}

void BackgroundFetchJobController::StartRequest(
    const BackgroundFetchRequestInfo& request) {
  DCHECK_EQ(state_, State::FETCHING);

  std::unique_ptr<DownloadUrlParameters> download_parameters(
      base::MakeUnique<DownloadUrlParameters>(
          request.GetURL(), storage_partition_->GetURLRequestContext()));

  // TODO(peter): The |download_parameters| should be populated with all the
  // properties set in the |request|'s ServiceWorkerFetchRequest member.

  download_parameters->set_callback(
      base::Bind(&BackgroundFetchJobController::DidStartRequest,
                 weak_ptr_factory_.GetWeakPtr(), request));

  // TODO(peter): Move this call to the UI thread.
  // See https://codereview.chromium.org/2781623009/
  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser_context_);
  DCHECK(download_manager);

  download_manager->DownloadUrl(std::move(download_parameters));
}

void BackgroundFetchJobController::DidStartRequest(
    const BackgroundFetchRequestInfo& request,
    DownloadItem* download_item,
    DownloadInterruptReason interrupt_reason) {
  DCHECK_EQ(interrupt_reason, DOWNLOAD_INTERRUPT_REASON_NONE);
  DCHECK(download_item);

  // Update the |request|'s download GUID in the DataManager.
  data_manager_->MarkRequestAsStarted(registration_id_, request,
                                      download_item->GetGuid());

  // Register for updates on the download's progress.
  download_item->AddObserver(this);

  // Associate the |download_item| with the |request| so that we can retrieve
  // it's information when further updates happen.
  downloads_.insert(std::make_pair(download_item, request));
}

void BackgroundFetchJobController::UpdateUI(const std::string& title) {
  // TODO(harkness): Update the user interface with |title|.
}

void BackgroundFetchJobController::Abort() {
  // TODO(harkness): Abort all in-progress downloads.

  state_ = State::ABORTED;

  // Inform the owner of the controller about the job having completed.
  std::move(completed_callback_).Run(this);
}

void BackgroundFetchJobController::OnDownloadUpdated(DownloadItem* item) {
  auto iter = downloads_.find(item);
  DCHECK(iter != downloads_.end());

  const BackgroundFetchRequestInfo& request = iter->second;

  switch (item->GetState()) {
    case DownloadItem::DownloadState::COMPLETE:
      // TODO(peter): Populate the responses' information in the |request|.

      // Remove the |item| from the list of active downloads. Expect one more
      // completed file acknowledgement too, to prevent race conditions.
      pending_completed_file_acknowledgements_++;
      downloads_.erase(iter);

      item->RemoveObserver(this);

      // Mark the |request| as having completed and fetch the next request from
      // storage. This may also mean that we've completed the job.
      data_manager_->MarkRequestAsCompleteAndGetNextRequest(
          registration_id_, request,
          base::BindOnce(&BackgroundFetchJobController::DidGetNextRequest,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case DownloadItem::DownloadState::CANCELLED:
      // TODO(harkness): Consider how we want to handle cancelled downloads.
      break;
    case DownloadItem::DownloadState::INTERRUPTED:
      // TODO(harkness): Just update the notification that it is paused.
      break;
    case DownloadItem::DownloadState::IN_PROGRESS:
      // TODO(harkness): If the download was previously paused, this should now
      // unpause the notification.
      break;
    case DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      break;
  }
}

void BackgroundFetchJobController::DidGetNextRequest(
    const base::Optional<BackgroundFetchRequestInfo>& request) {
  DCHECK_LE(pending_completed_file_acknowledgements_, 1);
  pending_completed_file_acknowledgements_--;

  // If a |request| has been given, start downloading the file and bail.
  if (request) {
    StartRequest(request.value());
    return;
  }

  // If there are outstanding completed file acknowlegements, bail as well.
  if (pending_completed_file_acknowledgements_ > 0)
    return;

  state_ = State::COMPLETED;

  // Otherwise the job this controller is responsible for has completed.
  std::move(completed_callback_).Run(this);
}

void BackgroundFetchJobController::OnDownloadDestroyed(DownloadItem* item) {
  DCHECK_EQ(downloads_.count(item), 1u);
  downloads_.erase(item);

  item->RemoveObserver(this);
}

}  // namespace content
