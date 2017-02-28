// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context),
      background_fetch_job_controller_(browser_context, storage_partition),
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

  // TODO(harkness): Create the Download observer.
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchContext::CreateRequest(
    const BackgroundFetchJobInfo& job_info,
    std::vector<BackgroundFetchRequestInfo>& request_infos) {
  DCHECK_GE(1U, request_infos.size());
  // Inform the data manager about the new download.
  BackgroundFetchJobData* job_data =
      background_fetch_data_manager_.CreateRequest(job_info, request_infos);
  // If job_data is null, the DataManager will have logged an error.
  if (job_data)
    background_fetch_job_controller_.ProcessJob(job_info.guid(), job_data);
}

}  // namespace content
