// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"

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
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : browser_context_(browser_context),
      data_manager_(browser_context, service_worker_context),
      event_dispatcher_(service_worker_context),
      delegate_proxy_(browser_context_->GetBackgroundFetchDelegate()),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_.CreateRegistration(
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
  registration.developer_id = registration_id.developer_id();
  registration.unique_id = registration_id.unique_id();
  registration.icons = options.icons;
  registration.title = options.title;
  registration.download_total = options.download_total;

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          registration);
}

BackgroundFetchJobController* BackgroundFetchContext::GetActiveFetch(
    const std::string& unique_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = active_fetches_.find(unique_id);
  if (iter == active_fetches_.end())
    return nullptr;

  BackgroundFetchJobController* controller = iter->second.get();
  if (controller->state() == BackgroundFetchJobController::State::ABORTED ||
      controller->state() == BackgroundFetchJobController::State::COMPLETED) {
    return nullptr;
  }

  return controller;
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<BackgroundFetchJobController> controller =
      std::make_unique<BackgroundFetchJobController>(
          &delegate_proxy_, registration_id, options, &data_manager_,
          base::BindOnce(&BackgroundFetchContext::DidCompleteJob,
                         weak_factory_.GetWeakPtr()));

  // Start fetching the first few requests immediately. At some point in the
  // future we may want a more elaborate scheduling mechanism here.
  controller->Start();

  active_fetches_.insert(
      std::make_pair(registration_id.unique_id(), std::move(controller)));
}

void BackgroundFetchContext::DidCompleteJob(
    BackgroundFetchJobController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const BackgroundFetchRegistrationId& registration_id =
      controller->registration_id();

  // Delete observers for the registration, there won't be any future updates.
  registration_notifier_->RemoveObservers(registration_id.unique_id());

  DCHECK_GT(active_fetches_.count(registration_id.unique_id()), 0u);
  switch (controller->state()) {
    case BackgroundFetchJobController::State::ABORTED:
      event_dispatcher_.DispatchBackgroundFetchAbortEvent(
          registration_id,
          base::Bind(&BackgroundFetchContext::DeleteRegistration,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::vector<std::unique_ptr<BlobHandle>>()));
      return;
    case BackgroundFetchJobController::State::COMPLETED:
      data_manager_.GetSettledFetchesForRegistration(
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
    event_dispatcher_.DispatchBackgroundFetchedEvent(
        registration_id, std::move(settled_fetches),
        base::Bind(&BackgroundFetchContext::DeleteRegistration,
                   weak_factory_.GetWeakPtr(), registration_id,
                   std::move(blob_handles)));
  } else {
    event_dispatcher_.DispatchBackgroundFetchFailEvent(
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
  DCHECK_GT(active_fetches_.count(registration_id.unique_id()), 0u);

  // Delete all persistent information associated with the |registration_id|.
  data_manager_.DeleteRegistration(
      registration_id, base::BindOnce(&RecordRegistrationDeletedError));

  // Delete the local state associated with the |registration_id|.
  active_fetches_.erase(registration_id.unique_id());
}

}  // namespace content
