// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_picker.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

namespace {

// Records the final request status UMA for an offlining request. This should
// only be called once per Offliner::LoadAndSave request.
void RecordOfflinerResultUMA(Offliner::RequestStatus request_status) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.Background.OfflinerRequestStatus",
                            request_status,
                            Offliner::RequestStatus::STATUS_COUNT);
}

}  // namespace

RequestCoordinator::RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                                       std::unique_ptr<OfflinerFactory> factory,
                                       std::unique_ptr<RequestQueue> queue,
                                       std::unique_ptr<Scheduler> scheduler)
    : is_busy_(false),
      is_canceled_(false),
      offliner_(nullptr),
      policy_(std::move(policy)),
      factory_(std::move(factory)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      offliner_timeout_(base::TimeDelta::FromSeconds(
          policy_->GetSinglePageTimeLimitInSeconds())),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));
}

RequestCoordinator::~RequestCoordinator() {}

bool RequestCoordinator::SavePageLater(
    const GURL& url, const ClientId& client_id, bool was_user_requested) {
  DVLOG(2) << "URL is " << url << " " << __func__;

  // TODO(petewil): We need a robust scheme for allocating new IDs.
  static int64_t id = 0;

  // Build a SavePageRequest.
  offline_pages::SavePageRequest request(
      id++, url, client_id, base::Time::Now(), was_user_requested);

  // Put the request on the request queue.
  queue_->AddRequest(request,
                     base::Bind(&RequestCoordinator::AddRequestResultCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void RequestCoordinator::RemoveRequests(
    const std::vector<ClientId>& client_ids) {
  queue_->RemoveRequestsByClientId(
      client_ids, base::Bind(&RequestCoordinator::UpdateRequestCallback,
                             weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::AddRequestResultCallback(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {

  // Inform the scheduler that we have an outstanding task..
  scheduler_->Schedule(GetTriggerConditionsForUserRequest());
}

// Called in response to updating a request in the request queue.
void RequestCoordinator::UpdateRequestCallback(
    RequestQueue::UpdateRequestResult result) {
  // If the request succeeded, nothing to do.  If it failed, we can't really do
  // much, so just log it.
  if (result != RequestQueue::UpdateRequestResult::SUCCESS) {
    // TODO(petewil): Consider adding UMA or showing on offline-internals page.
    DLOG(WARNING) << "Failed to update a request retry count. "
                  << static_cast<int>(result);
  }
}

void RequestCoordinator::StopProcessing() {
  is_canceled_ = true;
  if (offliner_ && is_busy_)
    offliner_->Cancel();

  // Stopping offliner means it will not call callback.
  last_offlining_status_ =
      Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED;
  RecordOfflinerResultUMA(last_offlining_status_);

  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

// Returns true if the caller should expect a callback, false otherwise. For
// instance, this would return false if a request is already in progress.
bool RequestCoordinator::StartProcessing(
    const DeviceConditions& device_conditions,
    const base::Callback<void(bool)>& callback) {
  current_conditions_.reset(new DeviceConditions(device_conditions));
  if (is_busy_) return false;

  // Mark the time at which we started processing so we can check our time
  // budget.
  operation_start_time_ = base::Time::Now();

  is_canceled_ = false;
  scheduler_callback_ = callback;

  TryNextRequest();

  return true;
}

void RequestCoordinator::TryNextRequest() {
  // If there is no time left in the budget, return to the scheduler.
  // We do not remove the pending task that was set up earlier in case
  // we run out of time, so the background scheduler will return to us
  // at the next opportunity to run background tasks.
  if (base::Time::Now() - operation_start_time_ >
      base::TimeDelta::FromSeconds(
          policy_->GetBackgroundProcessingTimeBudgetSeconds())) {
    // Let the scheduler know we are done processing.
    scheduler_callback_.Run(true);

    return;
  }

  // Choose a request to process that meets the available conditions.
  // This is an async call, and returns right away.
  picker_->ChooseNextRequest(
      base::Bind(&RequestCoordinator::RequestPicked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&RequestCoordinator::RequestQueueEmpty,
                 weak_ptr_factory_.GetWeakPtr()),
      current_conditions_.get());
}

// Called by the request picker when a request has been picked.
void RequestCoordinator::RequestPicked(const SavePageRequest& request) {
  // Send the request on to the offliner.
  SendRequestToOffliner(request);
}

void RequestCoordinator::RequestQueueEmpty() {
  // Clear the outstanding "safety" task in the scheduler.
  scheduler_->Unschedule();
  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  // Check that offlining didn't get cancelled while performing some async
  // steps.
  if (is_canceled_)
    return;

  GetOffliner();
  if (!offliner_) {
    DVLOG(0) << "Unable to create Offliner. "
             << "Cannot background offline page.";
    return;
  }

  DCHECK(!is_busy_);
  is_busy_ = true;

  // Start the load and save process in the offliner (Async).
  offliner_->LoadAndSave(request,
                         base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                                    weak_ptr_factory_.GetWeakPtr()));

  // Start a watchdog timer to catch pre-renders running too long
  watchdog_timer_.Start(FROM_HERE, offliner_timeout_, this,
                        &RequestCoordinator::StopProcessing);
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED)
           << ", status: " << static_cast<int>(status) << ", " << __func__;
  DCHECK_NE(status, Offliner::RequestStatus::UNKNOWN);
  DCHECK_NE(status, Offliner::RequestStatus::LOADED);
  event_logger_.RecordSavePageRequestUpdated(
      request.client_id().name_space,
      "Saved",
      request.request_id());
  last_offlining_status_ = status;
  RecordOfflinerResultUMA(last_offlining_status_);
  watchdog_timer_.Stop();

  is_busy_ = false;

  int64_t new_attempt_count = request.attempt_count() + 1;

  // Remove the request from the queue if it either succeeded or exceeded the
  // max number of retries.
  if (status == Offliner::RequestStatus::SAVED
       || new_attempt_count > policy_->GetMaxTries()) {
    queue_->RemoveRequest(request.request_id(),
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr()));
  } else {
    // If we failed, but are not over the limit, update the request in the
    // queue.
    SavePageRequest updated_request(request);
    updated_request.set_attempt_count(new_attempt_count);
    updated_request.set_last_attempt_time(base::Time::Now());
    RequestQueue::UpdateRequestCallback update_callback =
        base::Bind(&RequestCoordinator::UpdateRequestCallback,
                   weak_ptr_factory_.GetWeakPtr());
    queue_->UpdateRequest(
        updated_request,
        base::Bind(&RequestCoordinator::UpdateRequestCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Determine whether we might try another request in this
  // processing window based on how the previous request completed.
  //
  // TODO(dougarnett): Need to split PRERENDERING_FAILED into separate
  // codes as to whether we should try another request or not.
  switch (status) {
    case Offliner::RequestStatus::SAVED:
    case Offliner::RequestStatus::SAVE_FAILED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:  // timeout
    case Offliner::RequestStatus::PRERENDERING_CANCELED:
      // Consider processing another request in this service window.
      TryNextRequest();
      break;
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::PRERENDERING_FAILED:
      // No further processing in this service window.
      break;
    default:
      // Make explicit choice about new status codes that actually reach here.
      // Their default is no further processing in this service window.
      DCHECK(false);
  }
}

const Scheduler::TriggerConditions
RequestCoordinator::GetTriggerConditionsForUserRequest() {
  Scheduler::TriggerConditions trigger_conditions(
      policy_->PowerRequiredForUserRequestedPage(),
      policy_->BatteryPercentageRequiredForUserRequestedPage(),
      policy_->UnmeteredNetworkRequiredForUserRequestedPage());
  return trigger_conditions;
}

void RequestCoordinator::GetOffliner() {
  if (!offliner_) {
    offliner_ = factory_->GetOffliner(policy_.get());
  }
}

}  // namespace offline_pages
