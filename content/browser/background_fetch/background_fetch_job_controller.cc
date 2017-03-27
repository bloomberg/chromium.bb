// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    const std::string& job_guid,
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    BackgroundFetchDataManager* data_manager,
    base::OnceClosure completed_closure)
    : job_guid_(job_guid),
      browser_context_(browser_context),
      storage_partition_(storage_partition),
      data_manager_(data_manager),
      completed_closure_(std::move(completed_closure)),
      weak_ptr_factory_(this) {}

BackgroundFetchJobController::~BackgroundFetchJobController() = default;

void BackgroundFetchJobController::Shutdown() {
  DownloadManager* manager =
      BrowserContext::GetDownloadManager(browser_context_);
  // For any downloads in progress, remove the observer.
  for (auto& guid_map_entry : download_guid_map_) {
    DownloadItem* item = manager->GetDownloadByGuid(guid_map_entry.first);
    // TODO(harkness): Follow up with DownloadManager team  about whether this
    // should be a DCHECK.
    if (item)
      item->RemoveObserver(this);
  }
  download_guid_map_.clear();

  // TODO(harkness): Write final status to the DataManager. After this call, the
  // |data_manager_| is no longer valid.
  data_manager_ = nullptr;
}

void BackgroundFetchJobController::StartProcessing() {
  DCHECK(data_manager_);

  const BackgroundFetchRequestInfo& fetch_request =
      data_manager_->GetNextBackgroundFetchRequestInfo(job_guid_);
  ProcessRequest(fetch_request);

  // Currently, this processes a single request at a time.
}

void BackgroundFetchJobController::DownloadStarted(
    const std::string& request_guid,
    DownloadItem* item,
    DownloadInterruptReason reason) {
  // Start observing the DownloadItem. No need to store the pointer because it
  // can be retrieved from the DownloadManager.
  item->AddObserver(this);
  download_guid_map_[item->GetGuid()] = request_guid;
  data_manager_->UpdateRequestDownloadGuid(job_guid_, request_guid,
                                           item->GetGuid());

  // TODO(harkness): If DownloadStarted is called with a real interrupt reason,
  // record that and don't mark it as in-progress.
}

void BackgroundFetchJobController::OnDownloadUpdated(DownloadItem* item) {
  DCHECK(download_guid_map_.find(item->GetGuid()) != download_guid_map_.end());

  // Update the state of the request on the DataManager. If the state is a final
  // state, this will also update the complete status of the JobInfo.
  const std::string& request_guid = download_guid_map_[item->GetGuid()];
  bool requests_remaining = data_manager_->UpdateRequestState(
      job_guid_, request_guid, item->GetState(), item->GetLastReason());

  switch (item->GetState()) {
    case DownloadItem::DownloadState::COMPLETE:
      // If the download completed, update the storage location and size.
      data_manager_->UpdateRequestStorageState(job_guid_, request_guid,
                                               item->GetTargetFilePath(),
                                               item->GetReceivedBytes());
    // Fall through.
    case DownloadItem::DownloadState::CANCELLED:
      // TODO(harkness): Tell the notification service to update the download
      // notification.

      if (requests_remaining) {
        ProcessRequest(
            data_manager_->GetNextBackgroundFetchRequestInfo(job_guid_));
      } else if (data_manager_->IsComplete(job_guid_)) {
        std::move(completed_closure_).Run();
      }
      break;
    case DownloadItem::DownloadState::INTERRUPTED:
      // TODO(harkness): Just update the notification that it is paused.
      break;
    case DownloadItem::DownloadState::IN_PROGRESS:
      // TODO(harkness): If the download was previously paused, this should now
      // unpause the notification.
      break;
    case DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
      // TODO(harkness): Consider how we want to handle errors here.
      break;
  }
}

void BackgroundFetchJobController::OnDownloadDestroyed(DownloadItem* item) {
  // This can be called as part of Shutdown. Make sure the observers are
  // removed.
  item->RemoveObserver(this);
}

void BackgroundFetchJobController::ProcessRequest(
    const BackgroundFetchRequestInfo& fetch_request) {
  // TODO(harkness): Check if the download is already in progress or completed.

  // Set up the download parameters and the OnStartedCallback.
  std::unique_ptr<DownloadUrlParameters> params(
      base::MakeUnique<DownloadUrlParameters>(
          fetch_request.url(), storage_partition_->GetURLRequestContext()));
  params->set_callback(
      base::Bind(&BackgroundFetchJobController::DownloadStarted,
                 weak_ptr_factory_.GetWeakPtr(), fetch_request.guid()));

  BrowserContext::GetDownloadManager(browser_context_)
      ->DownloadUrl(std::move(params));
}

}  // namespace content
