// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_pages_ukm_reporter.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

namespace {
const bool kUserRequest = true;
const bool kStartOfProcessing = true;
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

// Records the request status UMA for an offlining request. This should
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
  if (request_status == Offliner::RequestStatus::SAVED ||
      request_status == Offliner::RequestStatus::SAVED_ON_LAST_RETRY) {
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

// Records final request status UMA for an offlining request. This should only
// be called once per Offliner::LoadAndSave request. Every Offliner::LoadAndSave
// request should also call this once.
void RecordSavePageResultUMA(
    const ClientId& client_id,
    RequestNotifier::BackgroundSavePageResult request_status) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.FinalSavePageResult"),
      1,
      static_cast<int>(RequestNotifier::BackgroundSavePageResult::STATUS_COUNT),
      static_cast<int>(
          RequestNotifier::BackgroundSavePageResult::STATUS_COUNT) +
          1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(request_status));
}

// Records whether the request comes from CCT or not
void RecordSavePageResultCCTUMA(const ClientId& client_id,
                                const std::string& origin) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      AddHistogramSuffix(client_id, "OfflinePages.Background.SavePageFromCCT"),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddBoolean(!origin.empty());
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

// Record the network quality at request creation time per namespace.
void RecordSavePageLaterNetworkQuality(
    const ClientId& client_id,
    const net::EffectiveConnectionType effective_connection) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(
          client_id,
          "OfflinePages.Background.EffectiveConnectionType.SavePageLater"),
      1, net::EFFECTIVE_CONNECTION_TYPE_LAST - 1,
      net::EFFECTIVE_CONNECTION_TYPE_LAST,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(effective_connection);
}

// Record the network quality at request creation time per namespace.
void RecordNetworkQualityAtRequestStartForFailedRequest(
    const ClientId& client_id,
    const net::EffectiveConnectionType effective_connection) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(
          client_id,
          "OfflinePages.Background.EffectiveConnectionType.OffliningStartType"),
      1, net::EFFECTIVE_CONNECTION_TYPE_LAST - 1,
      net::EFFECTIVE_CONNECTION_TYPE_LAST,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(effective_connection);
}

// In case we start processing from SavePageLater, we need a callback, but there
// is nothing for it to do.
void EmptySchedulerCallback(bool started) {}

// Returns whether |result| is a successful result for a single request.
bool IsSingleSuccessResult(const UpdateRequestsResult* result) {
  return result->store_state == StoreState::LOADED &&
         result->item_statuses.size() == 1 &&
         result->item_statuses.at(0).second == ItemActionStatus::SUCCESS;
}

}  // namespace

RequestCoordinator::SavePageLaterParams::SavePageLaterParams()
    : user_requested(true),
      availability(RequestAvailability::ENABLED_FOR_OFFLINER) {}

RequestCoordinator::SavePageLaterParams::SavePageLaterParams(
    const SavePageLaterParams& other) {
  url = other.url;
  client_id = other.client_id;
  user_requested = other.user_requested;
  availability = other.availability;
  original_url = other.original_url;
  request_origin = other.request_origin;
}

RequestCoordinator::SavePageLaterParams::~SavePageLaterParams() = default;

RequestCoordinator::RequestCoordinator(
    std::unique_ptr<OfflinerPolicy> policy,
    std::unique_ptr<Offliner> offliner,
    std::unique_ptr<RequestQueue> queue,
    std::unique_ptr<Scheduler> scheduler,
    net::NetworkQualityEstimator::NetworkQualityProvider*
        network_quality_estimator,
    std::unique_ptr<OfflinePagesUkmReporter> ukm_reporter)
    : is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      state_(RequestCoordinatorState::IDLE),
      processing_state_(ProcessingWindowState::STOPPED),
      use_test_device_conditions_(false),
      offliner_(std::move(offliner)),
      policy_(std::move(policy)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      policy_controller_(new ClientPolicyController()),
      network_quality_estimator_(network_quality_estimator),
      ukm_reporter_(std::move(ukm_reporter)),
      network_quality_at_request_start_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      active_request_id_(0),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      scheduler_callback_(base::Bind(&EmptySchedulerCallback)),
      internal_start_processing_callback_(base::Bind(&EmptySchedulerCallback)),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  std::unique_ptr<CleanupTaskFactory> cleanup_factory(
      new CleanupTaskFactory(policy_.get(), this, &event_logger_));
  queue_->SetCleanupFactory(std::move(cleanup_factory));
  // If we exited with any items left in the OFFLINING state, move them back to
  // the AVAILABLE state, and update the UI by sending notifications.  Do this
  // before we cleanup, so any requests that are now OFFLINING which have
  // expired can be legitimate candidates for cleanup.
  queue_->ReconcileRequests(base::Bind(&RequestCoordinator::ReconcileCallback,
                                       weak_ptr_factory_.GetWeakPtr()));
  // Do a cleanup of expired or over tried requests at startup time.
  queue_->CleanupRequestQueue();
}

RequestCoordinator::~RequestCoordinator() {}

int64_t RequestCoordinator::SavePageLater(
    const SavePageLaterParams& save_page_later_params,
    const SavePageLaterCallback& save_page_later_callback) {
  DVLOG(2) << "URL is " << save_page_later_params.url << " " << __func__;

  if (!OfflinePageModel::CanSaveURL(save_page_later_params.url)) {
    DVLOG(1) << "Not able to save page for requested url: "
             << save_page_later_params.url;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(save_page_later_callback, AddRequestResult::URL_ERROR));
    return 0L;
  }

  int64_t id = store_utils::GenerateOfflineId();

  // Build a SavePageRequest.
  offline_pages::SavePageRequest request(
      id, save_page_later_params.url, save_page_later_params.client_id,
      base::Time::Now(), save_page_later_params.user_requested);
  request.set_original_url(save_page_later_params.original_url);
  request.set_request_origin(save_page_later_params.request_origin);

  // If the download manager is not done with the request, put it on the
  // disabled list.
  if (save_page_later_params.availability ==
      RequestAvailability::DISABLED_FOR_OFFLINER) {
    disabled_requests_.insert(id);
  }

  // Put the request on the request queue.
  queue_->AddRequest(
      request,
      base::Bind(&RequestCoordinator::AddRequestResultCallback,
                 weak_ptr_factory_.GetWeakPtr(), save_page_later_callback,
                 save_page_later_params.availability));

  // Record the network quality when this request is made.
  if (network_quality_estimator_) {
    RecordSavePageLaterNetworkQuality(
        save_page_later_params.client_id,
        network_quality_estimator_->GetEffectiveConnectionType());
  }

  // Record UKM for this page offlining attempt.
  if (ukm_reporter_) {
    ukm_reporter_->ReportUrlOfflineRequest(
        save_page_later_params.url,
        save_page_later_params.availability ==
            RequestAvailability::DISABLED_FOR_OFFLINER);
  }

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
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  callback.Run(std::move(requests));
}

void RequestCoordinator::StopOfflining(const CancelCallback& final_callback,
                                       Offliner::RequestStatus stop_status) {
  if (offliner_ && state_ == RequestCoordinatorState::OFFLINING) {
    DCHECK_NE(active_request_id_, 0);
    if (offliner_->Cancel(base::Bind(
            &RequestCoordinator::HandleCancelUpdateStatusCallback,
            weak_ptr_factory_.GetWeakPtr(), final_callback, stop_status))) {
      return;
    }
  }

  UpdateStatusForCancel(stop_status);
  final_callback.Run(active_request_id_);
}

void RequestCoordinator::GetRequestsForSchedulingCallback(
    GetRequestsResult result,
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
  // offliner to cancel.
  if (active_request_id_ != 0) {
    if (request_ids.end() !=
        std::find(request_ids.begin(), request_ids.end(), active_request_id_)) {
      StopOfflining(base::Bind(&RequestCoordinator::ResetActiveRequestCallback,
                               weak_ptr_factory_.GetWeakPtr()),
                    Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED);
      return true;
    }
  }

  return false;
}

void RequestCoordinator::UpdateRequestForAbortedAttempt(
    const SavePageRequest& request) {
  if (request.started_attempt_count() >= policy_->GetMaxStartedTries()) {
    const RequestNotifier::BackgroundSavePageResult result(
        RequestNotifier::BackgroundSavePageResult::START_COUNT_EXCEEDED);
    event_logger_.RecordDroppedSavePageRequest(request.client_id().name_space,
                                               result, request.request_id());
    RemoveAttemptedRequest(request, result);
  } else {
    MarkAttemptAborted(request.request_id(), request.client_id().name_space);
  }
}

void RequestCoordinator::RemoveAttemptedRequest(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult result) {
  std::vector<int64_t> remove_requests;
  remove_requests.push_back(request.request_id());
  queue_->RemoveRequests(remove_requests,
                         base::Bind(&RequestCoordinator::HandleRemovedRequests,
                                    weak_ptr_factory_.GetWeakPtr(), result));
  RecordAttemptCount(request, result);
}

void RequestCoordinator::MarkAttemptAborted(int64_t request_id,
                                            const std::string& name_space) {
  queue_->MarkAttemptAborted(
      request_id,
      base::Bind(&RequestCoordinator::MarkAttemptDone,
                 weak_ptr_factory_.GetWeakPtr(), request_id, name_space));
}

void RequestCoordinator::MarkAttemptDone(
    int64_t request_id,
    const std::string& name_space,
    std::unique_ptr<UpdateRequestsResult> result) {
  // If the request succeeded, notify observer. If it failed, we can't really
  // do much, so just log it.
  if (IsSingleSuccessResult(result.get())) {
    NotifyChanged(result->updated_items.at(0));
  } else {
    DVLOG(1) << "Failed to mark attempt: " << request_id;
    UpdateRequestResult request_result =
        result->store_state != StoreState::LOADED
            ? UpdateRequestResult::STORE_FAILURE
            : UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    event_logger_.RecordUpdateRequestFailed(name_space, request_result);
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
                 RequestNotifier::BackgroundSavePageResult::USER_CANCELED));

  // Record the network quality when this request is removed.
  if (network_quality_estimator_) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Background.EffectiveConnectionType.RemoveRequests",
        network_quality_estimator_->GetEffectiveConnectionType(),
        net::EFFECTIVE_CONNECTION_TYPE_LAST);
  }

  if (canceled)
    TryNextRequest(!kStartOfProcessing);
}

void RequestCoordinator::PauseRequests(
    const std::vector<int64_t>& request_ids) {
  bool canceled = CancelActiveRequestIfItMatches(request_ids);

  // Remove the paused requests from prioritized list.
  for (int64_t id : request_ids) {
    auto it = std::find(prioritized_requests_.begin(),
                        prioritized_requests_.end(), id);
    if (it != prioritized_requests_.end())
      prioritized_requests_.erase(it);
  }

  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&RequestCoordinator::UpdateMultipleRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  // Record the network quality when this request is paused.
  if (network_quality_estimator_) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Background.EffectiveConnectionType.PauseRequests",
        network_quality_estimator_->GetEffectiveConnectionType(),
        net::EFFECTIVE_CONNECTION_TYPE_LAST);
  }

  if (canceled)
    TryNextRequest(!kStartOfProcessing);
}

void RequestCoordinator::ResumeRequests(
    const std::vector<int64_t>& request_ids) {
  prioritized_requests_.insert(prioritized_requests_.end(), request_ids.begin(),
                               request_ids.end());
  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::AVAILABLE,
      base::Bind(&RequestCoordinator::UpdateMultipleRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  // Record the network quality when this request is resumed.
  if (network_quality_estimator_) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Background.EffectiveConnectionType.ResumeRequests",
        network_quality_estimator_->GetEffectiveConnectionType(),
        net::EFFECTIVE_CONNECTION_TYPE_LAST);
  }

  // Schedule a task, in case there is not one scheduled.
  ScheduleAsNeeded();
}

void RequestCoordinator::AddRequestResultCallback(
    const SavePageLaterCallback& save_page_later_callback,
    RequestAvailability availability,
    AddRequestResult result,
    const SavePageRequest& request) {
  NotifyAdded(request);
  // Inform the scheduler that we have an outstanding task.
  scheduler_->Schedule(GetTriggerConditions(kUserRequest));

  if (availability == RequestAvailability::DISABLED_FOR_OFFLINER) {
    // Mark attempt started (presuming it is disabled for background offliner
    // because foreground offlining is happening).
    queue_->MarkAttemptStarted(
        request.request_id(),
        base::Bind(&RequestCoordinator::MarkAttemptDone,
                   weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                   request.client_id().name_space));
  } else if (request.user_requested()) {
    StartImmediatelyIfConnected();
  }

  save_page_later_callback.Run(result);
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

void RequestCoordinator::ReconcileCallback(
    std::unique_ptr<UpdateRequestsResult> result) {
  for (const auto& request : result->updated_items) {
    RecordOfflinerResult(request, Offliner::RequestStatus::BROWSER_KILLED);
    NotifyChanged(request);
  }
}

void RequestCoordinator::HandleRemovedRequestsAndCallback(
    const RemoveRequestsCallback& callback,
    RequestNotifier::BackgroundSavePageResult status,
    std::unique_ptr<UpdateRequestsResult> result) {
  // TODO(dougarnett): Define status code for user/api cancel and use here
  // to determine whether to record cancel time UMA.
  for (const auto& request : result->updated_items)
    RecordCancelTimeUMA(request);
  callback.Run(result->item_statuses);
  HandleRemovedRequests(status, std::move(result));
}

void RequestCoordinator::HandleRemovedRequests(
    RequestNotifier::BackgroundSavePageResult status,
    std::unique_ptr<UpdateRequestsResult> result) {
  for (const auto& request : result->updated_items)
    NotifyCompleted(request, status);
}

void RequestCoordinator::HandleCancelUpdateStatusCallback(
    const CancelCallback& final_callback,
    Offliner::RequestStatus stop_status,
    const SavePageRequest& canceled_request) {
  if (stop_status == Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT ||
      stop_status == Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED) {
    // Consider watchdog timeout a completed attempt.
    UpdateRequestForCompletedAttempt(canceled_request, stop_status);
  } else {
    // Otherwise consider this stop an aborted attempt.
    UpdateRequestForAbortedAttempt(canceled_request);
  }

  RecordOfflinerResult(canceled_request, stop_status);
  UpdateStatusForCancel(stop_status);
  final_callback.Run(canceled_request.request_id());
}

void RequestCoordinator::UpdateStatusForCancel(
    Offliner::RequestStatus stop_status) {
  // Stopping offliner means it will not call callback so set last status.
  last_offlining_status_ = stop_status;
  active_request_id_ = 0;
  state_ = RequestCoordinatorState::IDLE;
}

void RequestCoordinator::ResetActiveRequestCallback(int64_t offline_id) {
  active_request_id_ = 0;
}

void RequestCoordinator::StartSchedulerCallback(int64_t offline_id) {
  scheduler_callback_.Run(true);
}

void RequestCoordinator::TryNextRequestCallback(int64_t offline_id) {
  TryNextRequest(!kStartOfProcessing);
}

void RequestCoordinator::ScheduleAsNeeded() {
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(
      base::Bind(&RequestCoordinator::GetRequestsForSchedulingCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::StopProcessing(Offliner::RequestStatus stop_status) {
  processing_state_ = ProcessingWindowState::STOPPED;
  StopOfflining(base::Bind(&RequestCoordinator::StartSchedulerCallback,
                           weak_ptr_factory_.GetWeakPtr()),
                stop_status);
}

void RequestCoordinator::HandleWatchdogTimeout() {
  Offliner::RequestStatus watchdog_status =
      Offliner::REQUEST_COORDINATOR_TIMED_OUT;
  if (offliner_->HandleTimeout(active_request_id_))
    return;
  StopOfflining(base::Bind(&RequestCoordinator::TryNextRequestCallback,
                           weak_ptr_factory_.GetWeakPtr()),
                watchdog_status);
}

// Returns true if the caller should expect a callback, false otherwise. For
// instance, this would return false if a request is already in progress.
bool RequestCoordinator::StartScheduledProcessing(
    const DeviceConditions& device_conditions,
    const base::Callback<void(bool)>& callback) {
  DVLOG(2) << "Scheduled " << __func__;
  current_conditions_.reset(new DeviceConditions(device_conditions));
  return StartProcessingInternal(ProcessingWindowState::SCHEDULED_WINDOW,
                                 callback);
}

// Returns true if the caller should expect a callback, false otherwise.
bool RequestCoordinator::StartImmediateProcessing(
    const base::Callback<void(bool)>& callback) {
  UpdateCurrentConditionsFromAndroid();
  OfflinerImmediateStartStatus immediate_start_status =
      TryImmediateStart(callback);
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.ImmediateStartStatus", immediate_start_status,
      RequestCoordinator::OfflinerImmediateStartStatus::STATUS_COUNT);
  return immediate_start_status == OfflinerImmediateStartStatus::STARTED;
}

// The current_conditions_ must be set sometime before calling
// StartProcessingInternal on all calling code paths.
bool RequestCoordinator::StartProcessingInternal(
    const ProcessingWindowState processing_state,
    const base::Callback<void(bool)>& callback) {
  if (state_ != RequestCoordinatorState::IDLE)
    return false;
  processing_state_ = processing_state;
  scheduler_callback_ = callback;

  // Mark the time at which we started processing so we can check our time
  // budget.
  operation_start_time_ = base::Time::Now();

  TryNextRequest(kStartOfProcessing);

  return true;
}

void RequestCoordinator::StartImmediatelyIfConnected() {
  StartImmediateProcessing(internal_start_processing_callback_);
}

RequestCoordinator::OfflinerImmediateStartStatus
RequestCoordinator::TryImmediateStart(
    const base::Callback<void(bool)>& callback) {
  DVLOG(2) << "Immediate " << __func__;
  // Make sure not already busy processing.
  if (state_ == RequestCoordinatorState::OFFLINING)
    return OfflinerImmediateStartStatus::BUSY;

  // Make sure we are not on svelte device to start immediately.
  if (is_low_end_device_ &&
      !offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled()) {
    DVLOG(2) << "low end device, returning";
    // Let the scheduler know we are done processing and failed due to svelte.
    callback.Run(false);
    return OfflinerImmediateStartStatus::NOT_STARTED_ON_SVELTE;
  }

  if (current_conditions_->GetNetConnectionType() ==
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    RequestConnectedEventForStarting();
    return OfflinerImmediateStartStatus::NO_CONNECTION;
  }
  // Clear any pending connected event request since we have connection
  // and will start processing.
  ClearConnectedEventRequest();

  if (StartProcessingInternal(ProcessingWindowState::IMMEDIATE_WINDOW,
                              callback)) {
    return OfflinerImmediateStartStatus::STARTED;
  }
  return OfflinerImmediateStartStatus::NOT_ACCEPTED;
}

void RequestCoordinator::RequestConnectedEventForStarting() {
  connection_notifier_.reset(new ConnectionNotifier(
      base::Bind(&RequestCoordinator::HandleConnectedEventForStarting,
                 weak_ptr_factory_.GetWeakPtr())));
}

void RequestCoordinator::ClearConnectedEventRequest() {
  connection_notifier_.reset(nullptr);
}

void RequestCoordinator::HandleConnectedEventForStarting() {
  ClearConnectedEventRequest();
  StartImmediatelyIfConnected();
}

void RequestCoordinator::UpdateCurrentConditionsFromAndroid() {
  // If we have already set the connection type for testing, don't get it from
  // android, but use what the test already set up.
  if (use_test_device_conditions_)
    return;

  current_conditions_ = std::make_unique<DeviceConditions>(
      scheduler_->GetCurrentDeviceConditions());
}

void RequestCoordinator::TryNextRequest(bool is_start_of_processing) {
  state_ = RequestCoordinatorState::PICKING;

  // If this is the first call, the device conditions are current, no need to
  // update them.
  if (!is_start_of_processing) {
    // Get current device conditions from the Java side across the bridge.
    // NetworkChangeNotifier will not have the right conditions if chromium is
    // in the background in android, so prefer to always get the conditions via
    // the android APIs.
    UpdateCurrentConditionsFromAndroid();
  }

  base::TimeDelta processing_time_budget;
  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetWhenBackgroundScheduledInSeconds());
  } else {
    DCHECK(processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW);
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetForImmediateLoadInSeconds());
  }

  net::NetworkChangeNotifier::ConnectionType connection_type =
      current_conditions_->GetNetConnectionType();

  // If there is no network or no time left in the budget, return to the
  // scheduler. We do not remove the pending scheduler task that was set
  // up earlier in case we run out of time, so the background scheduler
  // will return to us at the next opportunity to run background tasks.
  if (connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE ||
      (base::Time::Now() - operation_start_time_) > processing_time_budget) {
    state_ = RequestCoordinatorState::IDLE;

    // If we were doing immediate processing, try to start it again
    // when we get connected.
    if (processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW)
      RequestConnectedEventForStarting();

    // Let the scheduler know we are done processing.
    scheduler_callback_.Run(true);
    DVLOG(2) << " out of time, giving up. " << __func__;

    return;
  }

  // Ask request queue to make a new PickRequestTask object, then put it on
  // the task queue.
  queue_->PickNextRequest(
      policy_.get(),
      base::Bind(&RequestCoordinator::RequestPicked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&RequestCoordinator::RequestNotPicked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&RequestCoordinator::RequestCounts,
                 weak_ptr_factory_.GetWeakPtr(), is_start_of_processing),
      *current_conditions_.get(), disabled_requests_, prioritized_requests_);
}

// Called by the request picker when a request has been picked.
void RequestCoordinator::RequestPicked(const SavePageRequest& request,
                                       bool cleanup_needed) {
  DVLOG(2) << request.url() << " " << __func__;

  // Make sure we were not stopped while picking, since any kind of cancel/stop
  // will reset the state back to IDLE.
  if (state_ == RequestCoordinatorState::PICKING) {
    state_ = RequestCoordinatorState::OFFLINING;
    // Send the request on to the offliner.
    SendRequestToOffliner(request);
  }

  // Schedule a queue cleanup if needed.
  if (cleanup_needed)
    queue_->CleanupRequestQueue();
}

void RequestCoordinator::RequestNotPicked(
    bool non_user_requested_tasks_remaining,
    bool cleanup_needed) {
  DVLOG(2) << __func__;
  state_ = RequestCoordinatorState::IDLE;

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

  // Schedule a queue cleanup if needed.
  if (cleanup_needed)
    queue_->CleanupRequestQueue();

  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::RequestCounts(bool is_start_of_processing,
                                       size_t total_requests,
                                       size_t available_requests) {
  // Only capture request counts for the start of processing (not for
  // continued processing in the same window).
  if (!is_start_of_processing)
    return;

  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    if (is_low_end_device_) {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.AvailableRequestCount."
          "Svelte",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.UnavailableRequestCount."
          "Svelte",
          total_requests - available_requests);
    } else {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.AvailableRequestCount",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.UnavailableRequestCount",
          total_requests - available_requests);
    }
  } else if (processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW) {
    if (is_low_end_device_) {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.AvailableRequestCount."
          "Svelte",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.UnavailableRequestCount."
          "Svelte",
          total_requests - available_requests);
    } else {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.AvailableRequestCount",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.UnavailableRequestCount",
          total_requests - available_requests);
    }
  }
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  DCHECK(state_ == RequestCoordinatorState::OFFLINING);
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
    state_ = RequestCoordinatorState::IDLE;
    StopProcessing(Offliner::QUEUE_UPDATE_FAILED);
    DVLOG(1) << "Failed to mark attempt started: " << request_id;
    UpdateRequestResult request_result =
        update_result->store_state != StoreState::LOADED
            ? UpdateRequestResult::STORE_FAILURE
            : UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    event_logger_.RecordUpdateRequestFailed(client_namespace, request_result);
    return;
  }

  active_request_id_ = request_id;
  network_quality_at_request_start_ =
      network_quality_estimator_->GetEffectiveConnectionType();

  // Start the load and save process in the offliner (Async).
  if (offliner_->LoadAndSave(
          update_result->updated_items.at(0),
          base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&RequestCoordinator::OfflinerProgressCallback,
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

    // Inform observer of active request.
    NotifyChanged(update_result->updated_items.at(0));

    // Start a watchdog timer to catch offliners running too long
    watchdog_timer_.Start(FROM_HERE, timeout, this,
                          &RequestCoordinator::HandleWatchdogTimeout);
  } else {
    state_ = RequestCoordinatorState::IDLE;
    DVLOG(0) << "Unable to start LoadAndSave";
    StopProcessing(Offliner::LOADING_NOT_ACCEPTED);

    // We need to undo the MarkAttemptStarted that brought us to this
    // method since we didn't success in starting after all.
    MarkAttemptAborted(request_id, client_namespace);
  }
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED)
           << ", status: " << static_cast<int>(status) << ", " << __func__;
  DCHECK_NE(status, Offliner::RequestStatus::UNKNOWN);
  DCHECK_NE(status, Offliner::RequestStatus::LOADED);
  RecordOfflinerResult(request, status);
  last_offlining_status_ = status;
  watchdog_timer_.Stop();
  state_ = RequestCoordinatorState::IDLE;
  active_request_id_ = 0;

  UpdateRequestForCompletedAttempt(request, status);
  if (ShouldTryNextRequest(status))
    TryNextRequest(!kStartOfProcessing);
  else
    scheduler_callback_.Run(true);
}

void RequestCoordinator::OfflinerProgressCallback(
    const SavePageRequest& request,
    int64_t received_bytes) {
  DVLOG(2) << "offliner progress, received bytes: " << received_bytes;
  DCHECK(received_bytes >= 0);
  NotifyNetworkProgress(request, received_bytes);
}

void RequestCoordinator::UpdateRequestForCompletedAttempt(
    const SavePageRequest& request,
    Offliner::RequestStatus status) {
  // If the request failed, report the connection type as of the start of the
  // request.
  if (status != Offliner::RequestStatus::SAVED &&
      status != Offliner::RequestStatus::SAVED_ON_LAST_RETRY) {
    RecordNetworkQualityAtRequestStartForFailedRequest(
        request.client_id(), network_quality_at_request_start_);
  }

  if (status == Offliner::RequestStatus::FOREGROUND_CANCELED ||
      status == Offliner::RequestStatus::LOADING_CANCELED) {
    // Update the request for the canceled attempt.
    // TODO(dougarnett): See if we can conclusively identify other attempt
    // aborted cases to treat this way (eg, for Render Process Killed).
    UpdateRequestForAbortedAttempt(request);
  } else if (status == Offliner::RequestStatus::SAVED ||
             status == Offliner::RequestStatus::SAVED_ON_LAST_RETRY) {
    // Remove the request from the queue if it succeeded.
    RemoveAttemptedRequest(request,
                           RequestNotifier::BackgroundSavePageResult::SUCCESS);
  } else if (status == Offliner::RequestStatus::LOADING_FAILED_NO_RETRY) {
    RemoveAttemptedRequest(
        request, RequestNotifier::BackgroundSavePageResult::LOADING_FAILURE);
  } else if (request.completed_attempt_count() + 1 >=
             policy_->GetMaxCompletedTries()) {
    // Remove from the request queue if we exceeded max retries. The +1
    // represents the request that just completed. Since we call
    // MarkAttemptCompleted within the if branches, the completed_attempt_count
    // has not yet been updated when we are checking the if condition.
    const RequestNotifier::BackgroundSavePageResult result(
        RequestNotifier::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED);
    event_logger_.RecordDroppedSavePageRequest(request.client_id().name_space,
                                               result, request.request_id());
    RemoveAttemptedRequest(request, result);
  } else {
    // If we failed, but are not over the limit, update the request in the
    // queue.
    queue_->MarkAttemptCompleted(
        request.request_id(),
        base::Bind(&RequestCoordinator::MarkAttemptDone,
                   weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                   request.client_id().name_space));
  }
}

bool RequestCoordinator::ShouldTryNextRequest(
    Offliner::RequestStatus previous_request_status) {
  switch (previous_request_status) {
    case Offliner::RequestStatus::SAVED:
    case Offliner::RequestStatus::SAVE_FAILED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
    case Offliner::RequestStatus::LOADING_FAILED:
    case Offliner::RequestStatus::LOADING_FAILED_NO_RETRY:
      return true;
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::LOADING_CANCELED:
    case Offliner::RequestStatus::LOADING_FAILED_NO_NEXT:
      // No further processing in this service window.
      return false;
    case Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT:
    case Offliner::RequestStatus::SAVED_ON_LAST_RETRY:
      // If we timed out, check to see that there is time budget.
      return processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW;
    default:
      // Make explicit choice about new status codes that actually reach here.
      // Their default is no further processing in this service window.
      NOTREACHED();
      return false;
  }
}

void RequestCoordinator::EnableForOffliner(int64_t request_id,
                                           const ClientId& client_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;

  // Clear from disabled list.
  disabled_requests_.erase(request_id);

  // Mark the request as now in available state.
  MarkAttemptAborted(request_id, client_id.name_space);

  // If we are not busy, start processing right away.
  StartImmediatelyIfConnected();
}

void RequestCoordinator::MarkRequestCompleted(int64_t request_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;

  // Clear from disabled list.
  disabled_requests_.erase(request_id);

  // Remove the request, but send out SUCCEEDED instead of removed.
  // Note: since it had been disabled, it will not have been active in a
  // background offliner, so it is not appropriate to TryNextRequest here.
  std::vector<int64_t> request_ids { request_id };
  queue_->RemoveRequests(
      request_ids,
      base::Bind(&RequestCoordinator::HandleRemovedRequests,
                 weak_ptr_factory_.GetWeakPtr(),
                 RequestNotifier::BackgroundSavePageResult::SUCCESS));
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

void RequestCoordinator::NotifyCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  RecordSavePageResultUMA(request.client_id(), status);
  RecordSavePageResultCCTUMA(request.client_id(), request.request_origin());
  for (Observer& observer : observers_)
    observer.OnCompleted(request, status);
}

void RequestCoordinator::NotifyChanged(const SavePageRequest& request) {
  for (Observer& observer : observers_)
    observer.OnChanged(request);
}

void RequestCoordinator::NotifyNetworkProgress(const SavePageRequest& request,
                                               int64_t received_bytes) {
  for (Observer& observer : observers_)
    observer.OnNetworkProgress(request, received_bytes);
}

ClientPolicyController* RequestCoordinator::GetPolicyController() {
  return policy_controller_.get();
}

void RequestCoordinator::RecordOfflinerResult(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  event_logger_.RecordOfflinerResult(request.client_id().name_space, status,
                                     request.request_id());
  RecordOfflinerResultUMA(request.client_id(), request.creation_time(), status);
}

void RequestCoordinator::Shutdown() {
  network_quality_estimator_ = nullptr;
}

}  // namespace offline_pages
