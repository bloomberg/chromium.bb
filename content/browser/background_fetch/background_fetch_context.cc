// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include "content/browser/background_fetch/fetch_request.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context),
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
  // TODO(harkness): Create the Batch manager.
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchContext::CreateRequest(const FetchRequest& fetch_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Inform the data manager about the new download.
  background_fetch_data_manager_.CreateRequest(fetch_request);

  // TODO(harkness): Make the request to the download manager.
}

}  // namespace content
