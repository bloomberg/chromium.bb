// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_picker.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

namespace {
const bool kUserRequest = true;
const int kMinDurationSeconds = 1;
const int kMaxDurationSeconds = 7 * 24 * 60 * 60;  // 7 days
const int kDurationBuckets = 50;
const int kDisabledTaskRecheckSeconds = 5;

// TODO(dougarnett): Move to util location and share with model impl.
std::string AddHistogramSuffix(const ClientId& client_id,
                               const char* histogram_name) {
  if (client_id.name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += "." + client_id.name_space;
  return adjusted_histogram_name;
}

// Records the final request status UMA for an offlining request. This should
// only be called once per Offliner::LoadAndSave request.
void RecordOfflinerResultUMA(const ClientId& client_id,
                             const base::Time& request_creation_time,
                             Offliner::RequestStatus request_status) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.OfflinerRequestStatus"),
      1, static_cast<int>(Offliner::RequestStatus::STATUS_COUNT),
      static_cast<int>(Offliner::RequestStatus::STATUS_COUNT) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(request_status));

  // For successful requests also record time from request to save.
  if (request_status == Offliner::RequestStatus::SAVED) {
    // Using regular histogram (with dynamic suffix) rather than time-oriented
    // one to record samples in seconds rather than milliseconds.
    base::HistogramBase* histogram = base::Histogram::FactoryGet(
        AddHistogramSuffix(client_id, "OfflinePages.Background.TimeToSaved"),
        kMinDurationSeconds, kMaxDurationSeconds, kDurationBuckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    base::TimeDelta duration = base::Time::Now() - request_creation_time;
    histogram->Add(duration.InSeconds());
  }
}

void RecordStartTimeUMA(const SavePageRequest& request) {
  std::string histogram_name("OfflinePages.Background.TimeToStart");
  if (base::SysInfo::IsLowEndDevice()) {
    histogram_name += ".Svelte";
  }

  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_TIMES
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      AddHistogramSuffix(request.client_id(), histogram_name.c_str()),
      base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromDays(7), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  base::TimeDelta duration = base::Time::Now() - request.creation_time();
  histogram->AddTime(duration);
}

void RecordCancelTimeUMA(const SavePageRequest& canceled_request) {
  // Using regular histogram (with dynamic suffix) rather than time-oriented
  // one to record samples in seconds rather than milliseconds.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      AddHistogramSuffix(canceled_request.client_id(),
                         "OfflinePages.Background.TimeToCanceled"),
      kMinDurationSeconds, kMaxDurationSeconds, kDurationBuckets,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  base::TimeDelta duration =
      base::Time::Now() - canceled_request.creation_time();
  histogram->Add(duration.InSeconds());
}

// Records the number of started attempts for completed requests (whether
// successful or not).
void RecordAttemptCount(const SavePageRequest& request,
                        RequestNotifier::BackgroundSavePageResult status) {
  if (status == RequestNotifier::BackgroundSavePageResult::SUCCESS) {
    // TODO(dougarnett): Also record UMA for completed attempts here.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.Background.RequestSuccess.StartedAttemptCount",
        request.started_attempt_count(), 1, 10, 11);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.Background.RequestFailure.StartedAttemptCount",
        request.started_attempt_count(), 1, 10, 11);
  }
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

RequestCoordinator::RequestCoordinator(
    std::unique_ptr<OfflinerPolicy> policy,
    std::unique_ptr<OfflinerFactory> factory,
    std::unique_ptr<RequestQueue> queue,
    std::unique_ptr<Scheduler> scheduler,
    net::NetworkQualityEstimator::NetworkQualityProvider*
        network_quality_estimator)
    : is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      is_busy_(false),
      is_starting_(false),
      processing_state_(ProcessingWindowState::STOPPED),
      use_test_connection_type_(false),
      test_connection_type_(),
      offliner_(nullptr),
      policy_(std::move(policy)),
      factory_(std::move(factory)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      policy_controller_(new ClientPolicyController()),
      network_quality_estimator_(network_quality_estimator),
      active_request_(nullptr),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      scheduler_callback_(base::Bind(&EmptySchedulerCallback)),
      immediate_schedule_callback_(base::Bind(&EmptySchedulerCallback)),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  picker_.reset(
      new RequestPicker(queue_.get(), policy_.get(), this, &event_logger_));
}

RequestCoordinator::~RequestCoordinator() {}

int64_t RequestCoordinator::SavePageLater(const GURL& url,
                                          const ClientId& client_id,
                                          bool user_requested,
                                          RequestAvailability availability) {
  DVLOG(2) << "URL is " << url << " " << __func__;

  if (!OfflinePageModel::CanSaveURL(url)) {
    DVLOG(1) << "Not able to save page for requested url: " << url;
    return 0L;
  }

  int64_t id = GenerateOfflineId();

  // Build a SavePageRequest.
  offline_pages::SavePageRequest request(id, url, client_id, base::Time::Now(),
                                         user_requested);

  // If the download manager is not done with the request, put it on the
  // disabled list.
  if (availability == RequestAvailability::DISABLED_FOR_OFFLINER)
    disabled_requests_.insert(id);

  // Put the request on the request queue.
  queue_->AddRequest(request,
                     base::Bind(&RequestCoordinator::AddRequestResultCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  return id;
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
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  callback.Run(std::move(requests));
}

void RequestCoordinator::StopPrerendering(Offliner::RequestStatus stop_status) {
  if (offliner_ && is_busy_) {
    DCHECK(active_request_.get());
    offliner_->Cancel();

    AbortRequestAttempt(active_request_.get());
  }

  // Stopping offliner means it will not call callback so set last status.
  last_offlining_status_ = stop_status;

  if (active_request_) {
    event_logger_.RecordOfflinerResult(active_request_->client_id().name_space,
                                       last_offlining_status_,
                                       active_request_->request_id());
    RecordOfflinerResultUMA(active_request_->client_id(),
                            active_request_->creation_time(),
                            last_offlining_status_);
    is_busy_ = false;
    active_request_.reset();
  }
}

void RequestCoordinator::GetRequestsForSchedulingCallback(
    RequestQueue::GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  bool user_requested = false;

  // Examine all requests, if we find a user requested one, we will use the less
  // restrictive conditions for user_requested requests.  Otherwise we will use
  // the more restrictive non-user-requested conditions.
  for (const auto& request : requests) {
    if (request->user_requested()) {
      user_requested = true;
      break;
    }
  }

  // In the get callback, determine the least restrictive, and call
  // GetTriggerConditions based on that.
  scheduler_->Schedule(GetTriggerConditions(user_requested));
}

bool RequestCoordinator::CancelActiveRequestIfItMatches(
    const std::vector<int64_t>& request_ids) {
  // If we have a request in progress and need to cancel it, call the
  // pre-renderer to cancel.  TODO Make sure we remove any page created by the
  // prerenderer if it doesn't get the cancel in time.
  if (active_request_ != nullptr) {
    if (request_ids.end() != std::find(request_ids.begin(), request_ids.end(),
                                       active_request_->request_id())) {
      StopPrerendering(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED);
      active_request_.reset(nullptr);
      return true;
    }
  }

  return false;
}

void RequestCoordinator::AbortRequestAttempt(SavePageRequest* request) {
  if (request->started_attempt_count() >= policy_->GetMaxStartedTries()) {
    const BackgroundSavePageResult result(
        BackgroundSavePageResult::START_COUNT_EXCEEDED);
    event_logger_.RecordDroppedSavePageRequest(request->client_id().name_space,
                                               result, request->request_id());
    RemoveAttemptedRequest(*request, result);
  } else {
    queue_->MarkAttemptAborted(
        request->request_id(),
        base::Bind(&RequestCoordinator::MarkAttemptAbortedDone,
                   weak_ptr_factory_.GetWeakPtr(), request->request_id(),
                   request->client_id()));
  }
}

void RequestCoordinator::RemoveAttemptedRequest(
    const SavePageRequest& request,
    BackgroundSavePageResult result) {
  std::vector<int64_t> remove_requests;
  remove_requests.push_back(request.request_id());
  queue_->RemoveRequests(remove_requests,
                         base::Bind(&RequestCoordinator::HandleRemovedRequests,
                                    weak_ptr_factory_.GetWeakPtr(), result));
  RecordAttemptCount(request, result);
}

void RequestCoordinator::MarkAttemptAbortedDone(
    int64_t request_id,
    const ClientId& client_id,
    std::unique_ptr<UpdateRequestsResult> result) {
  // If the request succeeded, nothing to do.  If it failed, we can't really do
  // much, so just log it.
  if (result->store_state != StoreState::LOADED ||
      result->item_statuses.size() != 1 ||
      result->item_statuses.at(0).first != request_id ||
      result->item_statuses.at(0).second != ItemActionStatus::SUCCESS) {
    DVLOG(1) << "Failed to mark request aborted: " << request_id;
    event_logger_.RecordUpdateRequestFailed(
        client_id.name_space,
        result->store_state != StoreState::LOADED
            ? RequestQueue::UpdateRequestResult::STORE_FAILURE
            : RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
  }
}

void RequestCoordinator::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveRequestsCallback& callback) {
  bool canceled = CancelActiveRequestIfItMatches(request_ids);
  queue_->RemoveRequests(
      request_ids,
      base::Bind(&RequestCoordinator::HandleRemovedRequestsAndCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 BackgroundSavePageResult::REMOVED));
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
  // Schedule a task, in case there is not one scheduled.
  ScheduleAsNeeded();
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
  // Inform the scheduler that we have an outstanding task.
  scheduler_->Schedule(GetTriggerConditions(kUserRequest));

  if (request.user_requested())
    StartImmediatelyIfConnected();
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

void RequestCoordinator::UpdateMultipleRequestsCallback(
    std::unique_ptr<UpdateRequestsResult> result) {
  for (const auto& request : result->updated_items)
    NotifyChanged(request);

  bool available_user_request = false;
  for (const auto& request : result->updated_items) {
    if (!available_user_request && request.user_requested() &&
        request.request_state() == SavePageRequest::RequestState::AVAILABLE) {
      available_user_request = true;
    }
  }

  if (available_user_request)
    StartImmediatelyIfConnected();
}

// When we successfully remove a request that completed successfully, move on to
// the next request.
void RequestCoordinator::CompletedRequestCallback(
    const MultipleItemStatuses& status) {
  TryNextRequest();
}

void RequestCoordinator::HandleRemovedRequestsAndCallback(
    const RemoveRequestsCallback& callback,
    BackgroundSavePageResult status,
    std::unique_ptr<UpdateRequestsResult> result) {
  // TODO(dougarnett): Define status code for user/api cancel and use here
  // to determine whether to record cancel time UMA.
  for (const auto& request : result->updated_items)
    RecordCancelTimeUMA(request);
  callback.Run(result->item_statuses);
  HandleRemovedRequests(status, std::move(result));
}

void RequestCoordinator::HandleRemovedRequests(
    BackgroundSavePageResult status,
    std::unique_ptr<UpdateRequestsResult> result) {
  for (const auto& request : result->updated_items)
    NotifyCompleted(request, status);
}

void RequestCoordinator::ScheduleAsNeeded() {
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(
      base::Bind(&RequestCoordinator::GetRequestsForSchedulingCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::StopProcessing(
    Offliner::RequestStatus stop_status) {
  processing_state_ = ProcessingWindowState::STOPPED;
  StopPrerendering(stop_status);

  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::HandleWatchdogTimeout() {
  StopProcessing(Offliner::REQUEST_COORDINATOR_TIMED_OUT);
}

// Returns true if the caller should expect a callback, false otherwise. For
// instance, this would return false if a request is already in progress.
bool RequestCoordinator::StartProcessing(
    const DeviceConditions& device_conditions,
    const base::Callback<void(bool)>& callback) {
  return StartProcessingInternal(ProcessingWindowState::SCHEDULED_WINDOW,
                                 device_conditions, callback);
}

bool RequestCoordinator::StartProcessingInternal(
    const ProcessingWindowState processing_state,
    const DeviceConditions& device_conditions,
    const base::Callback<void(bool)>& callback) {
  current_conditions_.reset(new DeviceConditions(device_conditions));
  if (is_starting_ || is_busy_)
    return false;
  is_starting_ = true;
  processing_state_ = processing_state;
  scheduler_callback_ = callback;

  // Mark the time at which we started processing so we can check our time
  // budget.
  operation_start_time_ = base::Time::Now();

  TryNextRequest();

  return true;
}

void RequestCoordinator::StartImmediatelyIfConnected() {
  OfflinerImmediateStartStatus immediate_start_status = TryImmediateStart();
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.ImmediateStartStatus", immediate_start_status,
      RequestCoordinator::OfflinerImmediateStartStatus::STATUS_COUNT);
}

RequestCoordinator::OfflinerImmediateStartStatus
RequestCoordinator::TryImmediateStart() {
  // Make sure not already busy processing.
  if (is_busy_)
    return OfflinerImmediateStartStatus::BUSY;

  // Make sure we are not on svelte device to start immediately.
  if (is_low_end_device_) {
    DVLOG(2) << "low end device, returning";
    // Let the scheduler know we are done processing and failed due to svelte.
    immediate_schedule_callback_.Run(false);
    return OfflinerImmediateStartStatus::NOT_STARTED_ON_SVELTE;
  }

  if (network_quality_estimator_) {
    // TODO(dougarnett): Add UMA for quality type experienced.
    net::EffectiveConnectionType quality =
        network_quality_estimator_->GetEffectiveConnectionType();
    VLOG(2) << "TryImmediateStart:  Quality estimate "
        << static_cast<int>(quality);
  }

  if (GetConnectionType() ==
             net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE)
    return OfflinerImmediateStartStatus::NO_CONNECTION;

  // Start processing with manufactured conservative battery conditions
  // (i.e., assume no battery).
  // TODO(dougarnett): Obtain actual battery conditions (from Android/Java).

  DeviceConditions device_conditions(false, 0, GetConnectionType());
  if (StartProcessingInternal(ProcessingWindowState::IMMEDIATE_WINDOW,
                              device_conditions, immediate_schedule_callback_))
    return OfflinerImmediateStartStatus::STARTED;
  else
    return OfflinerImmediateStartStatus::NOT_ACCEPTED;
}

void RequestCoordinator::TryNextRequest() {
  base::TimeDelta processing_time_budget;
  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetWhenBackgroundScheduledInSeconds());
  } else {
    DCHECK(processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW);
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetForImmediateLoadInSeconds());
  }

  // If there is no time left in the budget, return to the scheduler.
  // We do not remove the pending task that was set up earlier in case
  // we run out of time, so the background scheduler will return to us
  // at the next opportunity to run background tasks.
  if ((base::Time::Now() - operation_start_time_) > processing_time_budget) {
    is_starting_ = false;

    // Let the scheduler know we are done processing.
    // TODO: Make sure the scheduler callback is valid before running it.
    scheduler_callback_.Run(true);
    DVLOG(2) << " out of time, giving up. " << __func__;

    return;
  }

  // Choose a request to process that meets the available conditions.
  // This is an async call, and returns right away.
  picker_->ChooseNextRequest(base::Bind(&RequestCoordinator::RequestPicked,
                                        weak_ptr_factory_.GetWeakPtr()),
                             base::Bind(&RequestCoordinator::RequestNotPicked,
                                        weak_ptr_factory_.GetWeakPtr()),
                             current_conditions_.get(),
                             disabled_requests_);
}

// Called by the request picker when a request has been picked.
void RequestCoordinator::RequestPicked(const SavePageRequest& request) {
  DVLOG(2) << request.url() << " " << __func__;
  is_starting_ = false;

  // Make sure we were not stopped while picking.
  if (processing_state_ != ProcessingWindowState::STOPPED) {
    // Send the request on to the offliner.
    SendRequestToOffliner(request);
  }
}

void RequestCoordinator::RequestNotPicked(
    bool non_user_requested_tasks_remaining) {
  DVLOG(2) << __func__;
  is_starting_ = false;

  // Clear the outstanding "safety" task in the scheduler.
  scheduler_->Unschedule();

  // If disabled tasks remain, post a new safety task for 5 sec from now.
  if (disabled_requests_.size() > 0) {
    scheduler_->BackupSchedule(GetTriggerConditions(kUserRequest),
                               kDisabledTaskRecheckSeconds);
  } else if (non_user_requested_tasks_remaining) {
    // If we don't have any of those, check for non-user-requested tasks.
    scheduler_->Schedule(GetTriggerConditions(!kUserRequest));
  }

  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  // Check that offlining didn't get cancelled while performing some async
  // steps.
  if (processing_state_ == ProcessingWindowState::STOPPED)
    return;

  GetOffliner();
  if (!offliner_) {
    DVLOG(0) << "Unable to create Offliner. "
             << "Cannot background offline page.";
    return;
  }

  DCHECK(!is_busy_);
  is_busy_ = true;

  // Record start time if this is first attempt.
  if (request.started_attempt_count() == 0) {
    RecordStartTimeUMA(request);
  }

  // Mark attempt started in the database and start offliner when completed.
  queue_->MarkAttemptStarted(
      request.request_id(),
      base::Bind(&RequestCoordinator::StartOffliner,
                 weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                 request.client_id().name_space));
}

void RequestCoordinator::StartOffliner(
    int64_t request_id,
    const std::string& client_namespace,
    std::unique_ptr<UpdateRequestsResult> update_result) {
  if (update_result->store_state != StoreState::LOADED ||
      update_result->item_statuses.size() != 1 ||
      update_result->item_statuses.at(0).first != request_id ||
      update_result->item_statuses.at(0).second != ItemActionStatus::SUCCESS) {
    is_busy_ = false;
    // TODO(fgorski): what is the best result? Do we create a new status?
    StopProcessing(Offliner::PRERENDERING_NOT_STARTED);
    DVLOG(1) << "Failed to mark attempt started: " << request_id;
    event_logger_.RecordUpdateRequestFailed(
        client_namespace,
        update_result->store_state != StoreState::LOADED
            ? RequestQueue::UpdateRequestResult::STORE_FAILURE
            : RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST);
    return;
  }

  // TODO(fgorski): Switch to request_id only, so that this value is not written
  // back to the store.
  active_request_.reset(
      new SavePageRequest(update_result->updated_items.at(0)));

  // Start the load and save process in the offliner (Async).
  if (offliner_->LoadAndSave(
          update_result->updated_items.at(0),
          base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                     weak_ptr_factory_.GetWeakPtr()))) {
    base::TimeDelta timeout;
    if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
      timeout = base::TimeDelta::FromSeconds(
          policy_->GetSinglePageTimeLimitWhenBackgroundScheduledInSeconds());
    } else {
      DCHECK(processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW);
      timeout = base::TimeDelta::FromSeconds(
          policy_->GetSinglePageTimeLimitForImmediateLoadInSeconds());
    }

    // Start a watchdog timer to catch pre-renders running too long
    watchdog_timer_.Start(FROM_HERE, timeout, this,
                          &RequestCoordinator::HandleWatchdogTimeout);
  } else {
    is_busy_ = false;
    DVLOG(0) << "Unable to start LoadAndSave";
    StopProcessing(Offliner::PRERENDERING_NOT_STARTED);
  }
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED)
           << ", status: " << static_cast<int>(status) << ", " << __func__;
  DCHECK_NE(status, Offliner::RequestStatus::UNKNOWN);
  DCHECK_NE(status, Offliner::RequestStatus::LOADED);
  event_logger_.RecordOfflinerResult(request.client_id().name_space, status,
                                     request.request_id());
  last_offlining_status_ = status;
  RecordOfflinerResultUMA(request.client_id(), request.creation_time(),
                          last_offlining_status_);
  watchdog_timer_.Stop();

  is_busy_ = false;
  active_request_.reset(nullptr);

  if (status == Offliner::RequestStatus::FOREGROUND_CANCELED ||
      status == Offliner::RequestStatus::PRERENDERING_CANCELED) {
    // Update the request for the canceled attempt.
    // TODO(dougarnett): See if we can conclusively identify other attempt
    // aborted cases to treat this way (eg, for Render Process Killed).
    SavePageRequest updated_request(request);
    AbortRequestAttempt(&updated_request);
    NotifyChanged(updated_request);
  } else if (status == Offliner::RequestStatus::SAVED) {
    // Remove the request from the queue if it succeeded.
    RemoveAttemptedRequest(request, BackgroundSavePageResult::SUCCESS);
  } else if (status == Offliner::RequestStatus::PRERENDERING_FAILED_NO_RETRY) {
    RemoveAttemptedRequest(request,
                           BackgroundSavePageResult::PRERENDER_FAILURE);
  } else if (request.completed_attempt_count() + 1 >=
             policy_->GetMaxCompletedTries()) {
    // Remove from the request queue if we exceeded max retries. The +1
    // represents the request that just completed. Since we call
    // MarkAttemptCompleted within the if branches, the completed_attempt_count
    // has not yet been updated when we are checking the if condition.
    const BackgroundSavePageResult result(
        BackgroundSavePageResult::RETRY_COUNT_EXCEEDED);
    event_logger_.RecordDroppedSavePageRequest(request.client_id().name_space,
                                               result, request.request_id());
    RemoveAttemptedRequest(request, result);
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
  switch (status) {
    case Offliner::RequestStatus::SAVED:
    case Offliner::RequestStatus::SAVE_FAILED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT:
    case Offliner::RequestStatus::PRERENDERING_FAILED_NO_RETRY:
      // Consider processing another request in this service window.
      TryNextRequest();
      break;
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::PRERENDERING_CANCELED:
    case Offliner::RequestStatus::PRERENDERING_FAILED:
      // No further processing in this service window.
      // Let the scheduler know we are done processing.
      scheduler_callback_.Run(true);
      break;
    default:
      // Make explicit choice about new status codes that actually reach here.
      // Their default is no further processing in this service window.
      NOTREACHED();
  }
}

void RequestCoordinator::EnableForOffliner(int64_t request_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;
  disabled_requests_.erase(request_id);
  // If we are not busy, start processing right away.
  StartImmediatelyIfConnected();
}

void RequestCoordinator::MarkRequestCompleted(int64_t request_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;
  disabled_requests_.erase(request_id);

  // Remove the request, but send out SUCCEEDED instead of removed.
  std::vector<int64_t> request_ids { request_id };
    queue_->RemoveRequests(
      request_ids,
      base::Bind(&RequestCoordinator::HandleRemovedRequestsAndCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Bind(&RequestCoordinator::CompletedRequestCallback,
                            weak_ptr_factory_.GetWeakPtr()),
                 BackgroundSavePageResult::SUCCESS));
}

const Scheduler::TriggerConditions RequestCoordinator::GetTriggerConditions(
    const bool user_requested) {
  return Scheduler::TriggerConditions(
      policy_->PowerRequired(user_requested),
      policy_->BatteryPercentageRequired(user_requested),
      policy_->UnmeteredNetworkRequired(user_requested));
}

void RequestCoordinator::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RequestCoordinator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RequestCoordinator::NotifyAdded(const SavePageRequest& request) {
  for (Observer& observer : observers_)
    observer.OnAdded(request);
}

void RequestCoordinator::NotifyCompleted(const SavePageRequest& request,
                                         BackgroundSavePageResult status) {
  for (Observer& observer : observers_)
    observer.OnCompleted(request, status);
}

void RequestCoordinator::NotifyChanged(const SavePageRequest& request) {
  for (Observer& observer : observers_)
    observer.OnChanged(request);
}

void RequestCoordinator::GetOffliner() {
  if (!offliner_) {
    offliner_ = factory_->GetOffliner(policy_.get());
  }
}

ClientPolicyController* RequestCoordinator::GetPolicyController() {
  return policy_controller_.get();
}

void RequestCoordinator::Shutdown() {
  network_quality_estimator_ = nullptr;
}

}  // namespace offline_pages
