// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_scheduler.h"

#include "base/guid.h"
#include "base/stl_util.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

using blink::mojom::BackgroundFetchError;
using blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchScheduler::BackgroundFetchScheduler(
    BackgroundFetchDataManager* data_manager,
    BackgroundFetchRegistrationNotifier* registration_notifier,
    BackgroundFetchDelegateProxy* delegate_proxy,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : data_manager_(data_manager),
      registration_notifier_(registration_notifier),
      delegate_proxy_(delegate_proxy),
      event_dispatcher_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {
  DCHECK(delegate_proxy_);
  delegate_proxy_->SetClickEventDispatcher(
      base::BindRepeating(&BackgroundFetchScheduler::DispatchClickEvent,
                          weak_ptr_factory_.GetWeakPtr()));
}

BackgroundFetchScheduler::~BackgroundFetchScheduler() = default;

void BackgroundFetchScheduler::ScheduleDownload() {
  DCHECK(!active_controller_);

  if (controller_ids_.empty())
    return;

  DCHECK(!job_controllers_.empty());

  std::string controller_id = controller_ids_.front();
  controller_ids_.pop_front();
  active_controller_ = job_controllers_[controller_id].get();
  DCHECK(active_controller_);

  data_manager_->PopNextRequest(
      active_controller_->registration_id(),
      base::BindOnce(&BackgroundFetchJobController::DidPopNextRequest,
                     active_controller_->GetWeakPtr()));
}

void BackgroundFetchScheduler::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchFailureReason failure_reason,
    blink::mojom::BackgroundFetchService::AbortCallback callback) {
  DCHECK_EQ(failure_reason,
            BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER);

  base::Erase(controller_ids_, registration_id.unique_id());

  auto it = job_controllers_.find(registration_id.unique_id());
  if (it == job_controllers_.end()) {
    std::move(callback).Run(BackgroundFetchError::INVALID_ID);
    return;
  }

  it->second->Abort(failure_reason, std::move(callback));
}

void BackgroundFetchScheduler::FinishJob(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchFailureReason failure_reason,
    base::OnceCallback<void(BackgroundFetchError)> callback) {
  bool job_started = false;
  if (active_controller_ &&
      active_controller_->registration_id() == registration_id) {
    active_controller_ = nullptr;
    job_started = true;
  }

  data_manager_->MarkRegistrationForDeletion(
      registration_id,
      /* check_for_failure= */ failure_reason ==
          BackgroundFetchFailureReason::NONE,
      base::BindOnce(&BackgroundFetchScheduler::DidMarkForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), registration_id,
                     job_started, std::move(callback)));

  auto it = job_controllers_.find(registration_id.unique_id());
  if (it != job_controllers_.end()) {
    completed_fetches_[it->first] = {registration_id,
                                     it->second->NewRegistration()};
    // Destroying the controller will stop all in progress tasks.
    job_controllers_.erase(it);
  }

  if (!active_controller_)
    ScheduleDownload();
}

void BackgroundFetchScheduler::DidMarkForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool job_started,
    base::OnceCallback<void(BackgroundFetchError)> callback,
    BackgroundFetchError error,
    BackgroundFetchFailureReason failure_reason) {
  DCHECK(callback);
  std::move(callback).Run(error);

  // It's normal to get INVALID_ID errors here - it means the registration was
  // already inactive (marked for deletion). This happens when an abort (from
  // developer or from user) races with the download completing/failing, or even
  // when two aborts race.
  if (error != BackgroundFetchError::NONE)
    return;

  auto it = completed_fetches_.find(registration_id.unique_id());
  DCHECK(it != completed_fetches_.end());

  blink::mojom::BackgroundFetchRegistrationPtr& registration =
      it->second.second;
  // Include any other failure reasons the marking for deletion may have found.
  if (registration->failure_reason == BackgroundFetchFailureReason::NONE)
    registration->failure_reason = failure_reason;

  registration->result =
      registration->failure_reason == BackgroundFetchFailureReason::NONE
          ? blink::mojom::BackgroundFetchResult::SUCCESS
          : blink::mojom::BackgroundFetchResult::FAILURE;

  registration_notifier_->Notify(*registration);

  event_dispatcher_.DispatchBackgroundFetchCompletionEvent(
      registration_id, registration.Clone(),
      base::BindOnce(&BackgroundFetchScheduler::CleanupRegistration,
                     weak_ptr_factory_.GetWeakPtr(), registration_id));

  if (!job_started ||
      registration->failure_reason ==
          BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
      registration->failure_reason ==
          BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER) {
    // No need to keep the controller around since there won't be dispatch
    // events.
    completed_fetches_.erase(it);
  }
}

void BackgroundFetchScheduler::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Indicate to the renderer that the records for this fetch are no longer
  // available.
  registration_notifier_->NotifyRecordsUnavailable(registration_id.unique_id());

  // Delete the data associated with this fetch. Cache storage will keep the
  // downloaded data around so long as there are references to it, and delete
  // it once there is none. We don't need to do that accounting.
  data_manager_->DeleteRegistration(registration_id, base::DoNothing());

  // Notify other systems that this registration is complete.
  delegate_proxy_->MarkJobComplete(registration_id.unique_id());
}

void BackgroundFetchScheduler::DispatchClickEvent(
    const std::string& unique_id) {
  // Case 1: The active fetch received a click event.
  if (active_controller_ &&
      active_controller_->registration_id().unique_id() == unique_id) {
    event_dispatcher_.DispatchBackgroundFetchClickEvent(
        active_controller_->registration_id(),
        active_controller_->NewRegistration(), base::DoNothing());
    return;
  }

  // Case 2: A completed fetch received a click event.
  auto it = completed_fetches_.find(unique_id);
  if (it == completed_fetches_.end())
    return;

  event_dispatcher_.DispatchBackgroundFetchClickEvent(
      it->second.first, std::move(it->second.second), base::DoNothing());
  completed_fetches_.erase(unique_id);
}

std::unique_ptr<BackgroundFetchJobController>
BackgroundFetchScheduler::CreateInitializedController(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistration& registration,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_completed_requests,
    int num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    bool start_paused) {
  // TODO(rayankans): Only create a controller when the fetch starts.
  auto controller = std::make_unique<BackgroundFetchJobController>(
      data_manager_, delegate_proxy_, registration_id, std::move(options), icon,
      registration.downloaded, registration.uploaded, registration.upload_total,
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_)),
      base::BindOnce(&BackgroundFetchScheduler::FinishJob,
                     weak_ptr_factory_.GetWeakPtr()));

  controller->InitializeRequestStatus(num_completed_requests, num_requests,
                                      std::move(active_fetch_requests),
                                      start_paused);

  return controller;
}

void BackgroundFetchScheduler::OnRegistrationCreated(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistration& registration,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_requests,
    bool start_paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  registration_notifier_->NoteTotalRequests(registration_id.unique_id(),
                                            num_requests);

  auto controller = CreateInitializedController(
      registration_id, registration, std::move(options), icon,
      /* completed_requests= */ 0, num_requests,
      /* active_fetch_requests= */ {}, start_paused);

  DCHECK_EQ(job_controllers_.count(registration_id.unique_id()), 0u);
  job_controllers_[registration_id.unique_id()] = std::move(controller);
  controller_ids_.push_back(registration_id.unique_id());
  if (!active_controller_)
    ScheduleDownload();
}

void BackgroundFetchScheduler::OnRegistrationLoadedAtStartup(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistration& registration,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_completed_requests,
    int num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controller = CreateInitializedController(
      registration_id, registration, std::move(options), icon,
      num_completed_requests, num_requests, active_fetch_requests,
      /* start_paused= */ false);

  // The current assumption is that there can be only one active job with one
  // active fetch.
  DCHECK(!active_controller_);
  active_controller_ = controller.get();
  job_controllers_[registration_id.unique_id()] = std::move(controller);

  DCHECK_LE(active_fetch_requests.size(), 1u);

  if (active_fetch_requests.empty()) {
    DCHECK_LT(num_completed_requests, num_requests);
    // Start processing the next request.
    data_manager_->PopNextRequest(
        active_controller_->registration_id(),
        base::BindOnce(&BackgroundFetchJobController::DidPopNextRequest,
                       active_controller_->GetWeakPtr()));
    return;
  }

  for (auto& request_info : active_fetch_requests) {
    active_controller_->StartRequest(
        std::move(request_info),
        base::BindOnce(&BackgroundFetchJobController::MarkRequestAsComplete,
                       active_controller_->GetWeakPtr()));
  }
}

void BackgroundFetchScheduler::OnRequestCompleted(
    const std::string& unique_id,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response) {
  registration_notifier_->NotifyRequestCompleted(unique_id, std::move(request),
                                                 std::move(response));
}

void BackgroundFetchScheduler::AbortFetches(
    int64_t service_worker_registration_id) {
  // Abandon all active associated with this service worker.
  // BackgroundFetchJobController::Abort() will eventually lead to deletion of
  // the controller from job_controllers, so the IDs need to be copied over.
  std::vector<BackgroundFetchJobController*> to_abort;
  for (const auto& controller : job_controllers_) {
    if (service_worker_registration_id !=
            blink::mojom::kInvalidServiceWorkerRegistrationId &&
        service_worker_registration_id !=
            controller.second->registration_id()
                .service_worker_registration_id()) {
      continue;
    }
    to_abort.push_back(controller.second.get());
  }

  for (auto* controller : to_abort) {
    // Erase it from |controller_ids_| first to avoid rescheduling.
    base::Erase(controller_ids_, controller->registration_id().unique_id());
    controller->Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
                      base::DoNothing());
  }
}

void BackgroundFetchScheduler::OnRegistrationQueried(
    blink::mojom::BackgroundFetchRegistration* registration) {
  DCHECK(registration);
  if (!active_controller_)
    return;

  // The data manager only has the number of bytes from completed downloads, so
  // augment this with the number of downloaded/uploaded bytes from in-progress
  // jobs.
  if (active_controller_->registration_id().unique_id() ==
      registration->unique_id) {
    registration->downloaded +=
        active_controller_->GetInProgressDownloadedBytes();
    registration->uploaded += active_controller_->GetInProgressUploadedBytes();
  }
}

void BackgroundFetchScheduler::OnServiceWorkerDatabaseCorrupted(
    int64_t service_worker_registration_id) {
  AbortFetches(service_worker_registration_id);
}

void BackgroundFetchScheduler::OnRegistrationDeleted(int64_t registration_id,
                                                     const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbortFetches(registration_id);
}

void BackgroundFetchScheduler::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbortFetches(blink::mojom::kInvalidServiceWorkerRegistrationId);
}

}  // namespace content
