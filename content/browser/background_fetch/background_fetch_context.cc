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
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<content::CacheStorageContextImpl>&
        cache_storage_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
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
      browser_context_, service_worker_context, cache_storage_context,
      std::move(quota_manager_proxy));
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
    CreateController(data.registration_id, data.registration, data.options,
                     data.icon, data.ui_title, data.num_completed_requests,
                     data.num_requests, std::move(data.active_fetch_requests));
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
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error,
                            base::nullopt /* BackgroundFetchRegistration */);
    return;
  }

  BackgroundFetchRegistration updated_registration(registration);

  // The data manager only has the number of bytes from completed downloads, so
  // augment this with the number of downloaded bytes from in-progress jobs.
  DCHECK(job_controllers_.count(registration.unique_id));
  updated_registration.downloaded +=
      job_controllers_[registration.unique_id]->GetInProgressDownloadedBytes();

  std::move(callback).Run(error, updated_registration);
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    RenderFrameHost* render_frame_host,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);
  int frame_tree_node_id =
      render_frame_host ? render_frame_host->GetFrameTreeNodeId() : 0;

  GetPermissionForOrigin(
      registration_id.origin(), render_frame_host,
      base::BindOnce(&BackgroundFetchContext::DidGetPermission,
                     weak_factory_.GetWeakPtr(), registration_id, requests,
                     options, icon, frame_tree_node_id));
}

void BackgroundFetchContext::GetPermissionForOrigin(
    const url::Origin& origin,
    RenderFrameHost* render_frame_host,
    GetPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceRequestInfo::WebContentsGetter wc_getter = base::NullCallback();

  // Permissions need to go through the DownloadRequestLimiter if the fetch
  // is started from a top-level frame.
  if (render_frame_host && !render_frame_host->GetParent()) {
    wc_getter = base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                                    render_frame_host->GetFrameTreeNodeId());
  }

  delegate_proxy_.GetPermissionForOrigin(origin, std::move(wc_getter),
                                         std::move(callback));
}

void BackgroundFetchContext::DidGetPermission(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    int frame_tree_node_id,
    bool has_permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&background_fetch::RecordBackgroundFetchUkmEvent,
                     registration_id.origin(), requests, options, icon,
                     frame_tree_node_id, has_permission));

  if (has_permission) {
    data_manager_->BackgroundFetchDataManager::CreateRegistration(
        registration_id, requests, options, icon,
        base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                       weak_factory_.GetWeakPtr(), registration_id));
    return;
  }

  // No permission, the fetch should be rejected.
  background_fetch::RecordRegistrationCreatedError(
      blink::mojom::BackgroundFetchError::PERMISSION_DENIED);
  std::move(fetch_callbacks_[registration_id])
      .Run(blink::mojom::BackgroundFetchError::PERMISSION_DENIED,
           base::nullopt);
}

void BackgroundFetchContext::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  background_fetch::RecordRegistrationCreatedError(error);

  auto iter = fetch_callbacks_.find(registration_id);

  // The fetch might have been abandoned already if the Service Worker was
  // unregistered or corrupted while registration was in progress.
  if (iter == fetch_callbacks_.end())
    return;

  if (error == blink::mojom::BackgroundFetchError::NONE)
    std::move(iter->second).Run(error, registration);
  else
    std::move(iter->second).Run(error, base::nullopt /* registration */);

  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The registration must a) still be active, or b) have completed/failed (not
  // aborted) with the waitUntil promise from that event not yet resolved.
  if (!job_controllers_.count(registration_id.unique_id())) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  data_manager_->UpdateRegistrationUI(registration_id, title, icon,
                                      std::move(callback));
}

void BackgroundFetchContext::OnServiceWorkerDatabaseCorrupted(
    int64_t service_worker_registration_id) {
  AbandonFetches(service_worker_registration_id);
}

void BackgroundFetchContext::OnQuotaExceeded(
    const BackgroundFetchRegistrationId& registration_id) {
  auto job_it = job_controllers_.find(registration_id.unique_id());
  if (job_it != job_controllers_.end() && job_it->second)
    job_it->second->Abort(BackgroundFetchReasonToAbort::QUOTA_EXCEEDED);
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

void BackgroundFetchContext::OnRegistrationCreated(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRegistration& registration,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    int num_requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (hang_registration_creation_for_testing_) {
    // Hang here, to allow time for testing races. For instance, this helps us
    // test the behavior when a service worker gets unregistered before the
    // controller can be created.
    return;
  }

  // TODO(peter): When this moves to the BackgroundFetchScheduler, only create
  // a controller when the background fetch can actually be started.

  CreateController(registration_id, registration, options, icon, options.title,
                   0u /* num_completed_requests */, num_requests,
                   {} /* active_fetch_requests */);
}

void BackgroundFetchContext::OnUpdatedUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = job_controllers_.find(registration_id.unique_id());
  if (iter != job_controllers_.end())
    iter->second->UpdateUI(title, icon);
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
    const BackgroundFetchRegistration& registration,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    const std::string& ui_title,
    size_t num_completed_requests,
    size_t num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controller = std::make_unique<BackgroundFetchJobController>(
      &delegate_proxy_, scheduler_.get(), registration_id, options, icon,
      registration.downloaded,
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_.get())),
      base::BindOnce(
          &BackgroundFetchContext::DidFinishJob, weak_factory_.GetWeakPtr(),
          base::Bind(&background_fetch::RecordSchedulerFinishedError)));

  controller->InitializeRequestStatus(num_completed_requests, num_requests,
                                      std::move(active_fetch_requests),
                                      ui_title);
  scheduler_->AddJobController(controller.get());
  job_controllers_.emplace(registration_id.unique_id(), std::move(controller));
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

  auto controllers_iter = job_controllers_.find(registration_id.unique_id());

  if (reason_to_abort == BackgroundFetchReasonToAbort::ABORTED_BY_DEVELOPER) {
    DCHECK(controllers_iter != job_controllers_.end());
    controllers_iter->second->Abort(reason_to_abort);
  }
  auto registration = controllers_iter->second->NewRegistration(
      blink::mojom::BackgroundFetchState::FAILURE);

  switch (reason_to_abort) {
    case BackgroundFetchReasonToAbort::ABORTED_BY_DEVELOPER:
    case BackgroundFetchReasonToAbort::CANCELLED_FROM_UI:
      CleanupRegistration(registration_id, {},
                          blink::mojom::BackgroundFetchState::FAILURE);
      event_dispatcher_.DispatchBackgroundFetchAbortEvent(
          registration_id, std::move(registration), base::DoNothing());
      return;
    case BackgroundFetchReasonToAbort::TOTAL_DOWNLOAD_SIZE_EXCEEDED:
    case BackgroundFetchReasonToAbort::SERVICE_WORKER_UNAVAILABLE:
    case BackgroundFetchReasonToAbort::QUOTA_EXCEEDED:
    case BackgroundFetchReasonToAbort::NONE:
      // This will send a BackgroundFetchFetched or BackgroundFetchFail event.
      // We still need this to figure out which event to send.
      // TODO(crbug.com/699957, crbug.com/874092): Add a method to only return
      // the information needed to dispatch these events, instead of settled
      // fetches.
      data_manager_->GetSettledFetchesForRegistration(
          registration_id,
          std::make_unique<BackgroundFetchRequestMatchParams>(),
          base::BindOnce(&BackgroundFetchContext::DidGetSettledFetches,
                         weak_factory_.GetWeakPtr(), registration_id,
                         std::move(registration)));
      return;
  }
}

void BackgroundFetchContext::DidGetSettledFetches(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration,
    blink::mojom::BackgroundFetchError error,
    bool background_fetch_succeeded,
    std::vector<BackgroundFetchSettledFetch> settled_fetches,
    std::vector<std::unique_ptr<storage::BlobDataHandle>> blob_data_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    CleanupRegistration(registration_id, {} /* fetches */,
                        blink::mojom::BackgroundFetchState::FAILURE,
                        true /* preserve_info_to_dispatch_click_event */);
    return;
  }

  DCHECK(job_controllers_.count(registration_id.unique_id()));

  if (job_controllers_[registration_id.unique_id()]->total_downloads() !=
      static_cast<int>(settled_fetches.size())) {
    // Something went wrong, and some information was lost.
    background_fetch_succeeded = false;
  }

  // The `backgroundfetchsuccess` event will be invoked when all requests in the
  // registration have completed successfully. In all other cases, the
  // `backgroundfetchfail` event will be invoked instead.
  if (background_fetch_succeeded) {
    registration->state = blink::mojom::BackgroundFetchState::SUCCESS;
    event_dispatcher_.DispatchBackgroundFetchSuccessEvent(
        registration_id, std::move(registration),
        base::BindOnce(
            &BackgroundFetchContext::CleanupRegistration,
            weak_factory_.GetWeakPtr(), registration_id,
            // The blob uuid is sent as part of |settled_fetches|. Bind
            // |blob_data_handles| to the callback to keep them alive
            // until the waitUntil event is resolved.
            std::move(blob_data_handles),
            blink::mojom::BackgroundFetchState::SUCCESS,
            true /* preserve_info_to_dispatch_click_event */));
  } else {
    registration->state = blink::mojom::BackgroundFetchState::FAILURE;
    event_dispatcher_.DispatchBackgroundFetchFailEvent(
        registration_id, std::move(registration),
        base::BindOnce(
            &BackgroundFetchContext::CleanupRegistration,
            weak_factory_.GetWeakPtr(), registration_id,
            // The blob uuid is sent as part of |settled_fetches|. Bind
            // |blob_data_handles| to the callback to keep them alive
            // until the waitUntil event is resolved.
            std::move(blob_data_handles),
            blink::mojom::BackgroundFetchState::FAILURE,
            true /* preserve_info_to_dispatch_click_event */));
  }
}

void BackgroundFetchContext::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<std::unique_ptr<storage::BlobDataHandle>>& blob_handles,
    blink::mojom::BackgroundFetchState background_fetch_state,
    bool preserve_info_to_dispatch_click_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If we had an active JobController, it is no longer necessary, as the
  // notification's UI can no longer be updated after the fetch is aborted, or
  // after the waitUntil promise of the
  // backgroundfetchsuccess/backgroundfetchfail event has been resolved. Store
  // the information we want to persist after the controller is gone, in
  // completed_fetches_.
  auto controllers_iter = job_controllers_.find(registration_id.unique_id());
  DCHECK(controllers_iter != job_controllers_.end());
  if (preserve_info_to_dispatch_click_event) {
    completed_fetches_[registration_id.unique_id()] = std::make_pair(
        registration_id,
        controllers_iter->second->NewRegistration(background_fetch_state));
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
        std::move(iter->second.second) /* registration */, base::DoNothing());
    completed_fetches_.erase(iter);
    return;
  }

  // The fetch is active, or has been aborted/cancelled.
  auto controllers_iter = job_controllers_.find(unique_id);
  if (controllers_iter == job_controllers_.end())
    return;
  // TODO(crbug.com/873630): Implement a background fetch state manager to
  // keep track of states, and stop hard-coding it here.
  auto registration = controllers_iter->second->NewRegistration(
      blink::mojom::BackgroundFetchState::PENDING);
  event_dispatcher_.DispatchBackgroundFetchClickEvent(
      controllers_iter->second->registration_id(), std::move(registration),
      base::DoNothing());
}

void BackgroundFetchContext::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->GetSettledFetchesForRegistration(
      registration_id, std::move(match_params),
      base::BindOnce(&BackgroundFetchContext::DidGetMatchingRequests,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::DidGetMatchingRequests(
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback,
    blink::mojom::BackgroundFetchError error,
    bool background_fetch_succeeded,
    std::vector<BackgroundFetchSettledFetch> settled_fetches,
    std::vector<std::unique_ptr<storage::BlobDataHandle>> blob_data_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/863016): Update to 0u once we've stopped sending an
  // uncached response.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    DCHECK_EQ(settled_fetches.size(), 1u);

  std::move(callback).Run(std::move(settled_fetches));
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
