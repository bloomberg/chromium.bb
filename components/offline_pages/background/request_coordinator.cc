// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_picker.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

namespace {

// Records the final request status UMA for an offlining request. This should
// only be called once per Offliner::LoadAndSave request.
void RecordOfflinerResultUMA(const ClientId& client_id,
                             Offliner::RequestStatus request_status) {
  // TODO(dougarnett): Consider exposing AddHistogramSuffix from
  // offline_page_model_impl.cc as visible utility method.
  std::string histogram_name("OfflinePages.Background.OfflinerRequestStatus");
  if (!client_id.name_space.empty()) {
    histogram_name += "." + client_id.name_space;
  }

  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1,
      static_cast<int>(Offliner::RequestStatus::STATUS_COUNT),
      static_cast<int>(Offliner::RequestStatus::STATUS_COUNT) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(request_status));
}

// This should use the same algorithm as we use for OfflinePageItem, so the IDs
// are similar.
int64_t GenerateOfflineId() {
  return base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;
}

// In case we start processing from SavePageLater, we need a callback, but there
// is nothing for it to do.
void EmptySchedulerCallback(bool started) {}

}  // namespace

RequestCoordinator::RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                                       std::unique_ptr<OfflinerFactory> factory,
                                       std::unique_ptr<RequestQueue> queue,
                                       std::unique_ptr<Scheduler> scheduler)
    : is_busy_(false),
      is_stopped_(false),
      use_test_connection_type_(false),
      test_connection_type_(),
      offliner_(nullptr),
      policy_(std::move(policy)),
      factory_(std::move(factory)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      active_request_(nullptr),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      offliner_timeout_(base::TimeDelta::FromSeconds(
          policy_->GetSinglePageTimeLimitInSeconds())),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), this));
}

RequestCoordinator::~RequestCoordinator() {}

bool RequestCoordinator::SavePageLater(
    const GURL& url, const ClientId& client_id, bool user_requested) {
  DVLOG(2) << "URL is " << url << " " << __func__;

  if (!OfflinePageModel::CanSaveURL(url)) {
    DVLOG(1) << "Not able to save page for requested url: " << url;
    return false;
  }

  int64_t id = GenerateOfflineId();

  // Build a SavePageRequest.
  offline_pages::SavePageRequest request(id, url, client_id, base::Time::Now(),
                                         user_requested);

  // Put the request on the request queue.
  queue_->AddRequest(request,
                     base::Bind(&RequestCoordinator::AddRequestResultCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  return true;
}
void RequestCoordinator::GetAllRequests(const GetRequestsCallback& callback) {
  // Get all matching requests from the request queue, send them to our
  // callback.  We bind the namespace and callback to the front of the callback
  // param set.
  queue_->GetRequests(base::Bind(&RequestCoordinator::GetQueuedRequestsCallback,
                                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void RequestCoordinator::GetQueuedRequestsCallback(
    const GetRequestsCallback& callback,
    RequestQueue::GetRequestsResult result,
    const std::vector<SavePageRequest>& requests) {
  callback.Run(requests);
}

void RequestCoordinator::StopPrerendering() {
  if (offliner_ && is_busy_) {
    offliner_->Cancel();
    // Find current request and mark attempt aborted.
    active_request_->MarkAttemptAborted();
    queue_->UpdateRequest(*(active_request_.get()),
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     active_request_->client_id()));
  }

  // Stopping offliner means it will not call callback.
  last_offlining_status_ =
      Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED;

  if (active_request_) {
    RecordOfflinerResultUMA(active_request_->client_id(),
                            last_offlining_status_);
    is_busy_ = false;
    active_request_.reset();
  }

}

bool RequestCoordinator::CancelActiveRequestIfItMatches(
    const std::vector<int64_t>& request_ids) {
  // If we have a request in progress and need to cancel it, call the
  // pre-renderer to cancel.  TODO Make sure we remove any page created by the
  // prerenderer if it doesn't get the cancel in time.
  if (active_request_ != nullptr) {
    if (request_ids.end() != std::find(request_ids.begin(), request_ids.end(),
                                       active_request_->request_id())) {
      StopPrerendering();
      return true;
    }
  }

  return false;
}

void RequestCoordinator::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveRequestsCallback& callback) {
  bool canceled = CancelActiveRequestIfItMatches(request_ids);
  queue_->RemoveRequests(
      request_ids,
      base::Bind(&RequestCoordinator::HandleRemovedRequestsAndCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 SavePageStatus::REMOVED));
  if (canceled)
    TryNextRequest();
}

void RequestCoordinator::PauseRequests(
    const std::vector<int64_t>& request_ids) {
  bool canceled = CancelActiveRequestIfItMatches(request_ids);
  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&RequestCoordinator::UpdateMultipleRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  if (canceled)
    TryNextRequest();
}

void RequestCoordinator::ResumeRequests(
    const std::vector<int64_t>& request_ids) {
  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::AVAILABLE,
      base::Bind(&RequestCoordinator::UpdateMultipleRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  scheduler_->Schedule(GetTriggerConditionsForUserRequest());
}

net::NetworkChangeNotifier::ConnectionType
RequestCoordinator::GetConnectionType() {
  // If we have a connection type set for test, use that.
  if (use_test_connection_type_)
    return test_connection_type_;

  return net::NetworkChangeNotifier::GetConnectionType();
}

void RequestCoordinator::AddRequestResultCallback(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {
  NotifyAdded(request);
  // Inform the scheduler that we have an outstanding task..
  scheduler_->Schedule(GetTriggerConditionsForUserRequest());

  if (request.user_requested())
    StartProcessingIfConnected();
}

// Called in response to updating a request in the request queue.
void RequestCoordinator::UpdateRequestCallback(
    const ClientId& client_id,
    RequestQueue::UpdateRequestResult result) {
  // If the request succeeded, nothing to do.  If it failed, we can't really do
  // much, so just log it.
  if (result != RequestQueue::UpdateRequestResult::SUCCESS) {
    DVLOG(1) << "Failed to update request attempt details. "
             << static_cast<int>(result);
    event_logger_.RecordUpdateRequestFailed(client_id.name_space, result);
  }
}

// Called in response to updating multiple requests in the request queue.
void RequestCoordinator::UpdateMultipleRequestsCallback(
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  bool available_user_request = false;
  for (SavePageRequest request : requests) {
    NotifyChanged(request);
    if (!available_user_request && request.user_requested() &&
        request.request_state() == SavePageRequest::RequestState::AVAILABLE) {
      // TODO(dougarnett): Consider avoiding prospect of N^2 in case
      // size of bulk requests can get large (perhaps with easier to consume
      // callback interface).
      for (std::pair<int64_t, RequestQueue::UpdateRequestResult> pair :
           results) {
        if (pair.first == request.request_id() &&
            pair.second == RequestQueue::UpdateRequestResult::SUCCESS) {
          // We have a successfully updated, available, user request.
          available_user_request = true;
        }
      }
    }
  }

  if (available_user_request)
    StartProcessingIfConnected();
}

void RequestCoordinator::HandleRemovedRequestsAndCallback(
    const RemoveRequestsCallback& callback,
    SavePageStatus status,
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  callback.Run(results);
  HandleRemovedRequests(status, results, requests);
}

void RequestCoordinator::HandleRemovedRequests(
    SavePageStatus status,
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  for (SavePageRequest request : requests)
    NotifyCompleted(request, status);
}

void RequestCoordinator::StopProcessing() {
  is_stopped_ = true;
  StopPrerendering();

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

  is_stopped_ = false;
  scheduler_callback_ = callback;

  TryNextRequest();

  return true;
}

void RequestCoordinator::StartProcessingIfConnected() {
  // Makes sure not already busy processing.
  if (is_busy_) return;

  // Check for network connectivity.
  net::NetworkChangeNotifier::ConnectionType connection = GetConnectionType();

  if ((connection !=
       net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE)) {
    // Create conservative device conditions for the connectivity
    // (assume no battery).
    DeviceConditions device_conditions(false, 0, connection);
    StartProcessing(device_conditions, base::Bind(&EmptySchedulerCallback));
  }
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
  if (is_stopped_)
    return;

  GetOffliner();
  if (!offliner_) {
    DVLOG(0) << "Unable to create Offliner. "
             << "Cannot background offline page.";
    return;
  }

  DCHECK(!is_busy_);
  is_busy_ = true;

  // Prepare an updated request to attempt.
  SavePageRequest updated_request(request);
  updated_request.MarkAttemptStarted(base::Time::Now());
  active_request_.reset(new SavePageRequest(updated_request));

  // Start the load and save process in the offliner (Async).
  if (offliner_->LoadAndSave(
          updated_request, base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                                      weak_ptr_factory_.GetWeakPtr()))) {
    // Offliner accepted request so update it in the queue.
    queue_->UpdateRequest(updated_request,
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     updated_request.client_id()));

    // Start a watchdog timer to catch pre-renders running too long
    watchdog_timer_.Start(FROM_HERE, offliner_timeout_, this,
                          &RequestCoordinator::StopProcessing);
  } else {
    is_busy_ = false;
    DVLOG(0) << "Unable to start LoadAndSave";
    StopProcessing();
  }
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED)
           << ", status: " << static_cast<int>(status) << ", " << __func__;
  DCHECK_NE(status, Offliner::RequestStatus::UNKNOWN);
  DCHECK_NE(status, Offliner::RequestStatus::LOADED);
  event_logger_.RecordSavePageRequestUpdated(request.client_id().name_space,
                                             status, request.request_id());
  last_offlining_status_ = status;
  RecordOfflinerResultUMA(request.client_id(), last_offlining_status_);
  watchdog_timer_.Stop();

  is_busy_ = false;
  active_request_.reset(nullptr);

  if (status == Offliner::RequestStatus::FOREGROUND_CANCELED ||
      status == Offliner::RequestStatus::PRERENDERING_CANCELED) {
    // Update the request for the canceled attempt.
    // TODO(dougarnett): See if we can conclusively identify other attempt
    // aborted cases to treat this way (eg, for Render Process Killed).
    SavePageRequest updated_request(request);
    updated_request.MarkAttemptAborted();
    queue_->UpdateRequest(updated_request,
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     updated_request.client_id()));
    SavePageStatus notify_status =
        (status == Offliner::RequestStatus::FOREGROUND_CANCELED)
            ? SavePageStatus::FOREGROUND_CANCELED
            : SavePageStatus::PRERENDER_CANCELED;
    NotifyCompleted(updated_request, notify_status);

  } else if (status == Offliner::RequestStatus::SAVED) {
    // Remove the request from the queue if it succeeded.
    std::vector<int64_t> remove_requests;
    remove_requests.push_back(request.request_id());
    queue_->RemoveRequests(
        remove_requests,
        base::Bind(&RequestCoordinator::HandleRemovedRequests,
                   weak_ptr_factory_.GetWeakPtr(), SavePageStatus::SUCCESS));
  } else if (request.completed_attempt_count() + 1 >=
             policy_->GetMaxCompletedTries()) {
    // Remove from the request queue if we exceeeded max retries. The +1
    // represents the request that just completed. Since we call
    // MarkAttemptCompleted within the if branches, the completed_attempt_count
    // has not yet been updated when we are checking the if condition.
    std::vector<int64_t> remove_requests;
    remove_requests.push_back(request.request_id());
    queue_->RemoveRequests(
        remove_requests, base::Bind(&RequestCoordinator::HandleRemovedRequests,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    SavePageStatus::RETRY_COUNT_EXCEEDED));
  } else {
    // If we failed, but are not over the limit, update the request in the
    // queue.
    SavePageRequest updated_request(request);
    updated_request.MarkAttemptCompleted();
    queue_->UpdateRequest(updated_request,
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     updated_request.client_id()));
    NotifyChanged(updated_request);
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
      // Consider processing another request in this service window.
      TryNextRequest();
      break;
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::PRERENDERING_CANCELED:
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

void RequestCoordinator::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RequestCoordinator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RequestCoordinator::NotifyAdded(const SavePageRequest& request) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAdded(request));
}

void RequestCoordinator::NotifyCompleted(const SavePageRequest& request,
                                         SavePageStatus status) {
  FOR_EACH_OBSERVER(Observer, observers_, OnCompleted(request, status));
}

void RequestCoordinator::NotifyChanged(const SavePageRequest& request) {
  FOR_EACH_OBSERVER(Observer, observers_, OnChanged(request));
}

void RequestCoordinator::GetOffliner() {
  if (!offliner_) {
    offliner_ = factory_->GetOffliner(policy_.get());
  }
}

}  // namespace offline_pages
