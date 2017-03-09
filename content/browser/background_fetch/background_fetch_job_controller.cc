// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_job_data.h"
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
    std::unique_ptr<BackgroundFetchJobData> job_data)
    : browser_context_(browser_context),
      storage_partition_(storage_partition),
      job_data_(std::move(job_data)) {}

BackgroundFetchJobController::~BackgroundFetchJobController() = default;

void BackgroundFetchJobController::Shutdown() {
  // TODO(harkness): Remove any observers.
  // TODO(harkness): Write final status to the DataManager. After this call, the
  // |job_data_| is no longer valid.
  job_data_.reset(nullptr);
}

void BackgroundFetchJobController::StartProcessing() {
  DCHECK(job_data_);

  const BackgroundFetchRequestInfo& fetch_request =
      job_data_->GetNextBackgroundFetchRequestInfo();
  ProcessRequest(fetch_request);

  // Currently this only supports jobs of size 1.
  // TODO(crbug.com/692544)
  DCHECK(!job_data_->HasRequestsRemaining());
}

void BackgroundFetchJobController::ProcessRequest(
    const BackgroundFetchRequestInfo& fetch_request) {
  // TODO(harkness): Start the observer watching for this download.
  // TODO(harkness): Check if the download is already in progress or completed.

  // Send the fetch request to the DownloadManager.
  std::unique_ptr<DownloadUrlParameters> params(
      base::MakeUnique<DownloadUrlParameters>(
          fetch_request.url(), storage_partition_->GetURLRequestContext()));
  // TODO(harkness): Currently this is the only place the browser_context is
  // used. Evaluate whether we can just pass in the download manager explicitly.
  BrowserContext::GetDownloadManager(browser_context_)
      ->DownloadUrl(std::move(params));
}

}  // namespace content
