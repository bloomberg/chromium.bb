// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/bind_helpers.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

namespace {

void IgnoreError(blink::mojom::BackgroundFetchError) {
  // TODO(johnme): Log errors to UMA.
}

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
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(browser_context_->GetBackgroundFetchDelegate()),
      scheduler_(std::make_unique<BackgroundFetchScheduler>(&data_manager_)),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_.GetRegistration(
      service_worker_registration_id, origin, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  data_manager_.GetDeveloperIdsForServiceWorker(service_worker_registration_id,
                                                origin, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error, base::nullopt);
    return;
  }

  DCHECK(registration);
  // The data manager only has the number of bytes from completed downloads, so
  // augment this with the number of downloaded bytes from in-progress jobs.
  DCHECK(job_controllers_.count(registration->unique_id));
  registration->downloaded +=
      job_controllers_[registration->unique_id]->GetInProgressDownloadedBytes();
  std::move(callback).Run(error, *registration.get());
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
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RecordRegistrationCreatedError(error);
  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error, base::nullopt);
    return;
  }

  DCHECK(registration);
  // Create the BackgroundFetchJobController to do the actual fetching.
  CreateController(registration_id, options, *registration.get());
  std::move(callback).Run(error, *registration.get());
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const std::string& unique_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The registration must a) still be active, or b) have completed/failed (not
  // aborted) with the waitUntil promise from that event not yet resolved.
  if (!job_controllers_.count(unique_id)) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  data_manager_.UpdateRegistrationUI(
      unique_id, title,
      base::BindOnce(&BackgroundFetchContext::DidUpdateStoredUI,
                     weak_factory_.GetWeakPtr(), unique_id, title,
                     std::move(callback)));
}

void BackgroundFetchContext::DidUpdateStoredUI(
    const std::string& unique_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback,
    blink::mojom::BackgroundFetchError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): The controller might not exist if the developer updates the
  // UI from the event using event.waitUntil. Consider showing a message in the
  // console.
  if (error == blink::mojom::BackgroundFetchError::NONE &&
      job_controllers_.count(unique_id)) {
    job_controllers_[unique_id]->UpdateUI(title);
  }

  std::move(callback).Run(error);
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controller = std::make_unique<BackgroundFetchJobController>(
      &delegate_proxy_, registration_id, options, registration,
      scheduler_.get(),
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_.get())),
      base::BindOnce(&BackgroundFetchContext::DidFinishJob,
                     weak_factory_.GetWeakPtr(), base::Bind(&IgnoreError)));

  // TODO(delphick): This assumes that fetches are always started afresh in
  // each browser session. We need to initialize the number of downloads using
  // information loaded from the database.
  controller->InitializeRequestStatus(
      0, /* completed_downloads*/
      data_manager_.GetTotalNumberOfRequests(registration_id),
      std::vector<std::string>() /* outstanding download GUIDs */);

  scheduler_->AddJobController(controller.get());

  job_controllers_.insert(
      std::make_pair(registration_id.unique_id(), std::move(controller)));
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchService::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DidFinishJob(std::move(callback), registration_id, true /* aborted */);
}

void BackgroundFetchContext::DidFinishJob(
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    const BackgroundFetchRegistrationId& registration_id,
    bool aborted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If |aborted| is true, this will also propagate the event to any active
  // JobController for the registration, to terminate in-progress requests.
  data_manager_.MarkRegistrationForDeletion(
      registration_id,
      base::BindOnce(&BackgroundFetchContext::DidMarkForDeletion,
                     weak_factory_.GetWeakPtr(), registration_id, aborted,
                     std::move(callback)));
}

void BackgroundFetchContext::DidMarkForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool aborted,
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    blink::mojom::BackgroundFetchError error) {
  std::move(callback).Run(error);

  // It's normal to get INVALID_ID errors here - it means the registration was
  // already inactive (marked for deletion). This happens when an abort (from
  // developer or from user) races with the download completing/failing, or even
  // when two aborts race. TODO(johnme): Log STORAGE_ERRORs to UMA though.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  if (aborted) {
    DCHECK(job_controllers_.count(registration_id.unique_id()));
    job_controllers_[registration_id.unique_id()]->Abort();

    CleanupRegistration(registration_id, {});

    event_dispatcher_.DispatchBackgroundFetchAbortEvent(
        registration_id, base::Bind(&base::DoNothing));
  } else {
    data_manager_.GetSettledFetchesForRegistration(
        registration_id,
        base::BindOnce(&BackgroundFetchContext::DidGetSettledFetches,
                       weak_factory_.GetWeakPtr(), registration_id));
  }
}

void BackgroundFetchContext::DidGetSettledFetches(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    bool background_fetch_succeeded,
    std::vector<BackgroundFetchSettledFetch> settled_fetches,
    std::vector<std::unique_ptr<storage::BlobDataHandle>> blob_data_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    CleanupRegistration(registration_id, {});
    return;
  }

  // The `backgroundfetched` event will be invoked when all requests in the
  // registration have completed successfully. In all other cases, the
  // `backgroundfetchfail` event will be invoked instead.
  if (background_fetch_succeeded) {
    event_dispatcher_.DispatchBackgroundFetchedEvent(
        registration_id, std::move(settled_fetches),
        base::Bind(&BackgroundFetchContext::CleanupRegistration,
                   weak_factory_.GetWeakPtr(), registration_id,
                   // The blob uuid is sent as part of |settled_fetches|. Bind
                   // |blob_data_handles| to the callback to keep them alive
                   // until the waitUntil event is resolved.
                   std::move(blob_data_handles)));
  } else {
    event_dispatcher_.DispatchBackgroundFetchFailEvent(
        registration_id, std::move(settled_fetches),
        base::Bind(&BackgroundFetchContext::CleanupRegistration,
                   weak_factory_.GetWeakPtr(), registration_id,
                   // The blob uuid is sent as part of |settled_fetches|. Bind
                   // |blob_data_handles| to the callback to keep them alive
                   // until the waitUntil event is resolved.
                   std::move(blob_data_handles)));
  }
}

void BackgroundFetchContext::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<std::unique_ptr<storage::BlobDataHandle>>& blob_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If we had an active JobController, it is no longer necessary, as the
  // notification's UI can no longer be updated after the fetch is aborted, or
  // after the waitUntil promise of the backgroundfetched/backgroundfetchfail
  // event has been resolved.
  job_controllers_.erase(registration_id.unique_id());

  // At this point, JavaScript can no longer obtain BackgroundFetchRegistration
  // objects for this registration, and those objects are the only thing that
  // requires us to keep the registration's data around. So once the
  // RegistrationNotifier informs us that all existing observers (and hence
  // BackgroundFetchRegistration objects) have been garbage collected, it'll be
  // safe to delete the registration. This callback doesn't run if the browser
  // is shutdown before that happens - BackgroundFetchDataManager::Cleanup acts
  // as a fallback in that case, and deletes the registration on next startup.
  registration_notifier_->AddGarbageCollectionCallback(
      registration_id.unique_id(),
      base::Bind(&BackgroundFetchContext::LastObserverGarbageCollected,
                 weak_factory_.GetWeakPtr(), registration_id));
}

void BackgroundFetchContext::LastObserverGarbageCollected(
    const BackgroundFetchRegistrationId& registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_.DeleteRegistration(
      registration_id, base::BindOnce(&RecordRegistrationDeletedError));
}

}  // namespace content
