// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "url/origin.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : browser_context_(browser_context),
      storage_partition_(storage_partition),
      service_worker_context_(service_worker_context),
      background_fetch_data_manager_(
          base::MakeUnique<BackgroundFetchDataManager>(browser_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundFetchContext::~BackgroundFetchContext() {
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
  active_fetches_.clear();
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const blink::mojom::BackgroundFetchService::FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  background_fetch_data_manager_->CreateRegistration(
      registration_id, requests, options,
      base::BindOnce(&BackgroundFetchContext::DidCreateRegistration, this,
                     registration_id, options, callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const blink::mojom::BackgroundFetchService::FetchCallback& callback,
    blink::mojom::BackgroundFetchError error) {
  if (error != blink::mojom::BackgroundFetchError::NONE) {
    callback.Run(error, base::nullopt /* registration */);
    return;
  }

  // Create the BackgroundFetchJobController, which will do the actual fetching.
  CreateController(registration_id, options);

  // Create the BackgroundFetchRegistration the renderer process will receive,
  // which enables it to resolve the promise telling the developer it worked.
  BackgroundFetchRegistration registration;
  registration.tag = registration_id.tag();
  registration.icons = options.icons;
  registration.title = options.title;
  registration.total_download_size = options.total_download_size;

  callback.Run(blink::mojom::BackgroundFetchError::NONE, registration);
}

std::vector<std::string>
BackgroundFetchContext::GetActiveTagsForServiceWorkerRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin) const {
  std::vector<std::string> tags;
  for (const auto& pair : active_fetches_) {
    const BackgroundFetchRegistrationId& registration_id =
        pair.second->registration_id();

    // Only return the tags when the origin and SW registration id match.
    if (registration_id.origin() == origin &&
        registration_id.service_worker_registration_id() ==
            service_worker_registration_id) {
      tags.push_back(pair.second->registration_id().tag());
    }
  }

  return tags;
}

BackgroundFetchJobController* BackgroundFetchContext::GetActiveFetch(
    const BackgroundFetchRegistrationId& registration_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = active_fetches_.find(registration_id);
  if (iter == active_fetches_.end())
    return nullptr;

  return iter->second.get();
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options) {
  std::unique_ptr<BackgroundFetchJobController> controller =
      base::MakeUnique<BackgroundFetchJobController>(
          registration_id, options, browser_context_, storage_partition_,
          background_fetch_data_manager_.get(),
          base::BindOnce(&BackgroundFetchContext::DidFinishFetch, this));

  active_fetches_.insert(
      std::make_pair(registration_id, std::move(controller)));
}

void BackgroundFetchContext::DidFinishFetch(
    const BackgroundFetchRegistrationId& registration_id,
    bool aborted_by_developer) {
  DCHECK_GT(active_fetches_.count(registration_id), 0u);

  // TODO(peter): Dispatch the `backgroundfetched` or the `backgroundfetchfail`
  // event to the Service Worker when |aborted_by_developer| is not set.

  active_fetches_.erase(registration_id);
}

}  // namespace content
