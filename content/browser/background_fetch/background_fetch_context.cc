// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : browser_context_(browser_context),
      storage_partition_(storage_partition),
      service_worker_context_(service_worker_context),
      background_fetch_data_manager_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(harkness): BackgroundFetchContext should have
  // ServiceWorkerContextObserver as a parent class and should register as an
  // observer here.
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchContext::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BackgroundFetchContext::ShutdownOnIO, this));
}

void BackgroundFetchContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Call Shutdown on all pending job controllers to give them a chance to flush
  // any status to the DataManager.
  for (auto& job : job_map_)
    job.second->Shutdown();

  job_map_.clear();
}

void BackgroundFetchContext::CreateRequest(
    std::unique_ptr<BackgroundFetchJobInfo> job_info,
    std::vector<BackgroundFetchRequestInfo>& request_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GE(1U, request_infos.size());

  // Inform the data manager about the new download.
  const std::string job_guid = job_info->guid();
  std::unique_ptr<BackgroundFetchJobData> job_data =
      background_fetch_data_manager_.CreateRequest(std::move(job_info),
                                                   request_infos);

  // If job_data is null, the DataManager will have logged an error.
  if (job_data) {
    // Create a controller which drives the processing of the job. It will use
    // the JobData to get information about individual requests for the job.
    job_map_[job_guid] = base::MakeUnique<BackgroundFetchJobController>(
        job_guid, browser_context_, storage_partition_, std::move(job_data),
        base::BindOnce(&BackgroundFetchContext::DidCompleteJob, this,
                       job_guid));
  }
}

void BackgroundFetchContext::DidCompleteJob(const std::string& job_guid) {
  DCHECK(job_map_.find(job_guid) != job_map_.end());

  job_map_.erase(job_guid);

  // TODO(harkness): Once the caller receives the message, inform the
  // DataManager that it can clean up the pending job.
}

}  // namespace content
