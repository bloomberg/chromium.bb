// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchJobController::BackgroundFetchJobController(
    BrowserContext* browser_context,
    StoragePartition* storage_partition)
    : browser_context_(browser_context),
      storage_partition_(storage_partition) {}

BackgroundFetchJobController::~BackgroundFetchJobController() = default;

void BackgroundFetchJobController::ProcessJob(
    const std::string& job_guid,
    BackgroundFetchJobData* job_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(job_data);
  const BackgroundFetchRequestInfo& fetch_request =
      job_data->GetNextBackgroundFetchRequestInfo();
  ProcessRequest(job_guid, fetch_request);

  // Currently this only supports jobs of size 1.
  // TODO(crbug.com/692544)
  DCHECK(!job_data->HasRequestsRemaining());

  // TODO(harkness): Save the BackgroundFetchJobData so that when the download
  // completes we can notify the DataManager.
}

void BackgroundFetchJobController::ProcessRequest(
    const std::string& job_guid,
    const BackgroundFetchRequestInfo& fetch_request) {
  // First add the new request to the internal map tracking in-progress
  // requests.
  fetch_map_[job_guid] = fetch_request;

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
