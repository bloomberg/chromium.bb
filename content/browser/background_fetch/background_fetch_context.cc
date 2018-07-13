// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/bind_helpers.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<content::CacheStorageContextImpl>&
        cache_storage_context)
    : browser_context_(browser_context),
      service_worker_context_(service_worker_context),
      event_dispatcher_(service_worker_context),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(browser_context_->GetBackgroundFetchDelegate()),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_worker_context_);

  data_manager_ = std::make_unique<BackgroundFetchDataManager>(
      browser_context_, service_worker_context, cache_storage_context);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(data_manager_.get());
  delegate_proxy_.SetClickEventDispatcher(base::BindRepeating(
      &BackgroundFetchContext::DispatchClickEvent, weak_factory_.GetWeakPtr()));
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(this);
  data_manager_->RemoveObserver(this);
}

void BackgroundFetchContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->AddObserver(this);

  data_manager_->AddObserver(this);
  data_manager_->InitializeOnIOThread();
  data_manager_->GetInitializationData(
      base::BindOnce(&BackgroundFetchContext::DidGetInitializationData,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundFetchContext::DidGetInitializationData(
    blink::mojom::BackgroundFetchError error,
    std::vector<background_fetch::BackgroundFetchInitializationData>
        initialization_data) {
  if (error != blink::mojom::BackgroundFetchError::NONE) {
    // TODO(crbug.com/780025): Log failures to UMA.
    return;
  }

  for (auto& data : initialization_data) {
    CreateController(data.registration_id, data.options, data.icon,
                     data.ui_title, data.num_completed_requests,
                     data.num_requests, data.active_fetch_guids,
                     std::make_unique<BackgroundFetchRegistration>(
                         std::move(data.registration)));
  }
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->GetRegistration(
      service_worker_registration_id, origin, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  data_manager_->GetDeveloperIdsForServiceWorker(service_worker_registration_id,
                                                 origin, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error,
                            base::nullopt /* BackgroundFetchRegistration */);
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
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);

  data_manager_->CreateRegistration(
      registration_id, requests, options, icon,
      base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                     weak_factory_.GetWeakPtr(), registration_id, options, icon,
                     requests.size()));
}

void BackgroundFetchContext::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    size_t num_requests,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  background_fetch::RecordRegistrationCreatedError(error);
  if (error != blink::mojom::BackgroundFetchError::NONE) {
    DCHECK(fetch_callbacks_.count(registration_id));
    std::move(fetch_callbacks_[registration_id])
        .Run(error, base::nullopt /* BackgroundFetchRegistration */);
    fetch_callbacks_.erase(registration_id);
    return;
  }

  if (hang_registration_creation_for_testing_) {
    // Hang here, to allow time for testing races. For instance, this helps us
    // test the behavior when a service worker gets unregistered before the
    // controller can be created.
    return;
  }

  DCHECK(registration);

  // Create the BackgroundFetchJobController to do the actual fetching.
  CreateController(registration_id, options, icon, options.title,
                   0u /* num_completed_requests */, num_requests,
                   {} /* outstanding_guids */, std::move(registration));
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The registration must a) still be active, or b) have completed/failed (not
  // aborted) with the waitUntil promise from that event not yet resolved.
  if (!job_controllers_.count(registration_id.unique_id())) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  data_manager_->UpdateRegistrationUI(registration_id, title,
                                      std::move(callback));
}

void BackgroundFetchContext::OnServiceWorkerDatabaseCorrupted(
    int64_t service_worker_registration_id) {
  AbandonFetches(service_worker_registration_id);
}

void BackgroundFetchContext::AbandonFetches(
    int64_t service_worker_registration_id) {
  // Abandon all active fetches associated with this service worker.
  // BackgroundFetchJobController::Abort() will eventually lead to deletion of
  // the controller from job_controllers, hence we can't use a range based
  // for-loop here.
  for (auto iter = job_controllers_.begin(); iter != job_controllers_.end();
       /* no_increment */) {
    auto saved_iter = iter;
    iter++;
    if (service_worker_registration_id ==
            blink::mojom::kInvalidServiceWorkerRegistrationId ||
        saved_iter->second->registration_id()
                .service_worker_registration_id() ==
            service_worker_registration_id) {
      DCHECK(saved_iter->second);
      saved_iter->second->Abort(
          BackgroundFetchReasonToAbort::SERVICE_WORKER_UNAVAILABLE);
    }
  }

  for (auto iter = fetch_callbacks_.begin(); iter != fetch_callbacks_.end();
       /* no increment */) {
    if (service_worker_registration_id ==
            blink::mojom::kInvalidServiceWorkerRegistrationId ||
        iter->first.service_worker_registration_id() ==
            service_worker_registration_id) {
      DCHECK(iter->second);
      std::move(iter->second)
          .Run(blink::mojom::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE,
               base::nullopt /* BackgroundFetchRegistration */);
      iter = fetch_callbacks_.erase(iter);
    } else
      iter++;
  }
}

void BackgroundFetchContext::OnUpdatedUI(
    const BackgroundFetchRegistrationId& registration_id,
    const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = job_controllers_.find(registration_id.unique_id());
  if (iter != job_controllers_.end())
    iter->second->UpdateUI(title);
}

void BackgroundFetchContext::OnRegistrationDeleted(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbandonFetches(service_worker_registration_id);
}

void BackgroundFetchContext::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbandonFetches(blink::mojom::kInvalidServiceWorkerRegistrationId);
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    const std::string& ui_title,
    size_t num_completed_requests,
    size_t num_requests,
    const std::vector<std::string>& outstanding_guids,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controller = std::make_unique<BackgroundFetchJobController>(
      &delegate_proxy_, registration_id, options, icon,
      registration->downloaded, scheduler_.get(),
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_.get())),
      base::BindOnce(
          &BackgroundFetchContext::DidFinishJob, weak_factory_.GetWeakPtr(),
          base::Bind(&background_fetch::RecordSchedulerFinishedError)));

  controller->InitializeRequestStatus(num_completed_requests, num_requests,
                                      outstanding_guids, ui_title);
  scheduler_->AddJobController(controller.get());
  job_controllers_.emplace(registration_id.unique_id(), std::move(controller));

  auto fetch_callback_iter = fetch_callbacks_.find(registration_id);
  if (fetch_callback_iter != fetch_callbacks_.end()) {
    std::move(fetch_callback_iter->second)
        .Run(blink::mojom::BackgroundFetchError::NONE, *registration);
    fetch_callbacks_.erase(fetch_callback_iter);
  }
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchService::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DidFinishJob(std::move(callback), registration_id,
               BackgroundFetchReasonToAbort::ABORTED_BY_DEVELOPER);
}

void BackgroundFetchContext::DidFinishJob(
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchReasonToAbort reason_to_abort) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If |aborted| is true, this will also propagate the event to any active
  // JobController for the registration, to terminate in-progress requests.
  data_manager_->MarkRegistrationForDeletion(
      registration_id,
      base::BindOnce(&BackgroundFetchContext::DidMarkForDeletion,
                     weak_factory_.GetWeakPtr(), registration_id,
                     reason_to_abort, std::move(callback)));
}

void BackgroundFetchContext::DidMarkForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchReasonToAbort reason_to_abort,
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    blink::mojom::BackgroundFetchError error) {
  DCHECK(callback);
  std::move(callback).Run(error);

  // It's normal to get INVALID_ID errors here - it means the registration was
  // already inactive (marked for deletion). This happens when an abort (from
  // developer or from user) races with the download completing/failing, or even
  // when two aborts race. TODO(johnme): Log STORAGE_ERRORs to UMA though.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  if (reason_to_abort == BackgroundFetchReasonToAbort::ABORTED_BY_DEVELOPER) {
    DCHECK(job_controllers_.count(registration_id.unique_id()));
    job_controllers_[registration_id.unique_id()]->Abort(reason_to_abort);
  }

  switch (reason_to_abort) {
    case BackgroundFetchReasonToAbort::ABORTED_BY_DEVELOPER:
    case BackgroundFetchReasonToAbort::CANCELLED_FROM_UI:
      CleanupRegistration(registration_id, {},
                          mojom::BackgroundFetchState::FAILED);
      // TODO(rayankans): Send fetches to the event dispatcher.
      event_dispatcher_.DispatchBackgroundFetchAbortEvent(
          registration_id, {} /* settled_fetches */, base::DoNothing());
      return;
    case BackgroundFetchReasonToAbort::TOTAL_DOWNLOAD_SIZE_EXCEEDED:
    case BackgroundFetchReasonToAbort::SERVICE_WORKER_UNAVAILABLE:
    case BackgroundFetchReasonToAbort::NONE:
      // This will send a BackgroundFetchFetched or BackgroundFetchFail event.
      data_manager_->GetSettledFetchesForRegistration(
          registration_id,
          base::BindOnce(&BackgroundFetchContext::DidGetSettledFetches,
                         weak_factory_.GetWeakPtr(), registration_id));
      return;
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
    CleanupRegistration(registration_id, {} /* fetches */,
                        mojom::BackgroundFetchState::FAILED,
                        true /* preserve_info_to_dispatch_click_event */);
    return;
  }

  // The `backgroundfetched` event will be invoked when all requests in the
  // registration have completed successfully. In all other cases, the
  // `backgroundfetchfail` event will be invoked instead.
  if (background_fetch_succeeded) {
    event_dispatcher_.DispatchBackgroundFetchedEvent(
        registration_id, std::move(settled_fetches),
        base::BindOnce(
            &BackgroundFetchContext::CleanupRegistration,
            weak_factory_.GetWeakPtr(), registration_id,
            // The blob uuid is sent as part of |settled_fetches|. Bind
            // |blob_data_handles| to the callback to keep them alive
            // until the waitUntil event is resolved.
            std::move(blob_data_handles),
            mojom::BackgroundFetchState::SUCCEEDED,
            true /* preserve_info_to_dispatch_click_event */));
  } else {
    event_dispatcher_.DispatchBackgroundFetchFailEvent(
        registration_id, std::move(settled_fetches),
        base::BindOnce(
            &BackgroundFetchContext::CleanupRegistration,
            weak_factory_.GetWeakPtr(), registration_id,
            // The blob uuid is sent as part of |settled_fetches|. Bind
            // |blob_data_handles| to the callback to keep them alive
            // until the waitUntil event is resolved.
            std::move(blob_data_handles), mojom::BackgroundFetchState::FAILED,
            true /* preserve_info_to_dispatch_click_event */));
  }
}

void BackgroundFetchContext::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<std::unique_ptr<storage::BlobDataHandle>>& blob_handles,
    mojom::BackgroundFetchState background_fetch_state,
    bool preserve_info_to_dispatch_click_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If we had an active JobController, it is no longer necessary, as the
  // notification's UI can no longer be updated after the fetch is aborted, or
  // after the waitUntil promise of the backgroundfetched/backgroundfetchfail
  // event has been resolved. Store the information we want to persist after
  // the controller is gone, in completed_fetches_.
  scheduler_->RemoveJobController(registration_id);
  if (preserve_info_to_dispatch_click_event) {
    completed_fetches_[registration_id.unique_id()] = {registration_id,
                                                       background_fetch_state};
  }
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
      base::BindOnce(&BackgroundFetchContext::LastObserverGarbageCollected,
                     weak_factory_.GetWeakPtr(), registration_id));
}

void BackgroundFetchContext::DispatchClickEvent(const std::string& unique_id) {
  auto iter = completed_fetches_.find(unique_id);
  if (iter != completed_fetches_.end()) {
    // The fetch has succeeded or failed. (not aborted/cancelled).
    event_dispatcher_.DispatchBackgroundFetchClickEvent(
        iter->second.first /* registration_id */,
        iter->second.second /* background_fetch_state */, base::DoNothing());
    completed_fetches_.erase(iter);
    return;
  }

  // The fetch is active, or has been aborted/cancelled.
  auto controllers_iter = job_controllers_.find(unique_id);
  if (controllers_iter == job_controllers_.end())
    return;
  event_dispatcher_.DispatchBackgroundFetchClickEvent(
      controllers_iter->second->registration_id(),
      mojom::BackgroundFetchState::PENDING, base::DoNothing());
}

void BackgroundFetchContext::LastObserverGarbageCollected(
    const BackgroundFetchRegistrationId& registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->DeleteRegistration(
      registration_id,
      base::BindOnce(&background_fetch::RecordRegistrationDeletedError));
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchContext::ShutdownOnIO, this));
}

void BackgroundFetchContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->ShutdownOnIO();
}

void BackgroundFetchContext::SetDataManagerForTesting(
    std::unique_ptr<BackgroundFetchDataManager> data_manager) {
  DCHECK(data_manager);
  data_manager_ = std::move(data_manager);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(data_manager_.get());
}

}  // namespace content
