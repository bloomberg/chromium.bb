// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/origin.h"

namespace content {

namespace {

// Records the |error| status issued by the DataManager after it was requested
// to create and store a new Background Fetch registration.
void RecordRegistrationCreatedError(blink::mojom::BackgroundFetchError error) {
  // TODO(peter): Add UMA.
}

// Records the |error| status issued by the DataManager after the storage
// associated with a registration has been completely deleted.
void RecordRegistrationDeletedError(blink::mojom::BackgroundFetchError error) {
  // TODO(peter): Add UMA.
}

}  // namespace

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : browser_context_(browser_context),
      data_manager_(
          base::MakeUnique<BackgroundFetchDataManager>(browser_context)),
      event_dispatcher_(base::MakeUnique<BackgroundFetchEventDispatcher>(
          std::move(service_worker_context))),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchContext::InitializeOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  request_context_getter_ = request_context_getter;
  delegate_proxy_ = base::MakeUnique<BackgroundFetchDelegateProxy>(
      browser_context_, request_context_getter);
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->CreateRegistration(
      registration_id, requests, options,
      base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                     weak_factory_.GetWeakPtr(), registration_id, options,
                     std::move(callback)));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    blink::mojom::BackgroundFetchService::FetchCallback callback,
    blink::mojom::BackgroundFetchError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RecordRegistrationCreatedError(error);
  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error, base::nullopt /* registration */);
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

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          registration);
}

std::vector<std::string>
BackgroundFetchContext::GetActiveTagsForServiceWorkerRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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

  BackgroundFetchJobController* controller = iter->second.get();
  if (controller->state() == BackgroundFetchJobController::State::ABORTED ||
      controller->state() == BackgroundFetchJobController::State::COMPLETED) {
    return nullptr;
  }

  return controller;
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<BackgroundFetchJobController> controller =
      base::MakeUnique<BackgroundFetchJobController>(
          delegate_proxy_.get(), registration_id, options, data_manager_.get(),
          base::BindOnce(&BackgroundFetchContext::DidCompleteJob,
                         weak_factory_.GetWeakPtr()));

  // TODO(peter): We should actually be able to use Background Fetch in layout
  // tests. That requires a download manager and a request context.
  if (request_context_getter_) {
    // Start fetching the first few requests immediately. At some point in the
    // future we may want a more elaborate scheduling mechanism here.
    controller->Start();
  }

  active_fetches_.insert(
      std::make_pair(registration_id, std::move(controller)));
}

void BackgroundFetchContext::DidCompleteJob(
    BackgroundFetchJobController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const BackgroundFetchRegistrationId& registration_id =
      controller->registration_id();

  DCHECK_GT(active_fetches_.count(registration_id), 0u);
  switch (controller->state()) {
    case BackgroundFetchJobController::State::ABORTED:
      event_dispatcher_->DispatchBackgroundFetchAbortEvent(
          registration_id,
          base::Bind(&BackgroundFetchContext::DeleteRegistration,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::vector<std::unique_ptr<BlobHandle>>()));
      return;
    case BackgroundFetchJobController::State::COMPLETED:
      data_manager_->GetSettledFetchesForRegistration(
          registration_id,
          base::BindOnce(&BackgroundFetchContext::DidGetSettledFetches,
                         weak_factory_.GetWeakPtr(), registration_id));
      return;
    case BackgroundFetchJobController::State::INITIALIZED:
    case BackgroundFetchJobController::State::FETCHING:
      // These cases should not happen. Fall through to the NOTREACHED() below.
      break;
  }

  NOTREACHED();
}

void BackgroundFetchContext::DidGetSettledFetches(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    bool background_fetch_succeeded,
    std::vector<BackgroundFetchSettledFetch> settled_fetches,
    std::vector<std::unique_ptr<BlobHandle>> blob_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    DeleteRegistration(registration_id, std::move(blob_handles));
    return;
  }

  // The `backgroundfetched` event will be invoked when all requests in the
  // registration have completed successfully. In all other cases, the
  // `backgroundfetchfail` event will be invoked instead.
  if (background_fetch_succeeded) {
    event_dispatcher_->DispatchBackgroundFetchedEvent(
        registration_id, std::move(settled_fetches),
        base::Bind(&BackgroundFetchContext::DeleteRegistration,
                   weak_factory_.GetWeakPtr(), registration_id,
                   std::move(blob_handles)));
  } else {
    event_dispatcher_->DispatchBackgroundFetchFailEvent(
        registration_id, std::move(settled_fetches),
        base::Bind(&BackgroundFetchContext::DeleteRegistration,
                   weak_factory_.GetWeakPtr(), registration_id,
                   std::move(blob_handles)));
  }
}

void BackgroundFetchContext::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<std::unique_ptr<BlobHandle>>& blob_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(active_fetches_.count(registration_id), 0u);

  // Delete all persistent information associated with the |registration_id|.
  data_manager_->DeleteRegistration(
      registration_id, base::BindOnce(&RecordRegistrationDeletedError));

  // Delete the local state associated with the |registration_id|.
  active_fetches_.erase(registration_id);
}

}  // namespace content
