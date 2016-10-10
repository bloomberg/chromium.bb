// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_picker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/offline_pages/background/save_page_request.h"

namespace {
template <typename T>
int signum(T t) {
  return (T(0) < t) - (t < T(0));
}

#define CALL_MEMBER_FUNCTION(object, ptrToMember) ((object)->*(ptrToMember))
}  // namespace

namespace offline_pages {

RequestPicker::RequestPicker(RequestQueue* requestQueue,
                             OfflinerPolicy* policy,
                             RequestNotifier* notifier,
                             RequestCoordinatorEventLogger* event_logger)
    : queue_(requestQueue),
      policy_(policy),
      notifier_(notifier),
      event_logger_(event_logger),
      fewer_retries_better_(false),
      earlier_requests_better_(false),
      weak_ptr_factory_(this) {}

RequestPicker::~RequestPicker() {}

// Entry point for the async operation to choose the next request.
void RequestPicker::ChooseNextRequest(
    RequestCoordinator::RequestPickedCallback picked_callback,
    RequestCoordinator::RequestNotPickedCallback not_picked_callback,
    DeviceConditions* device_conditions,
    const std::set<int64_t>& disabled_requests) {
  picked_callback_ = picked_callback;
  not_picked_callback_ = not_picked_callback;
  fewer_retries_better_ = policy_->ShouldPreferUntriedRequests();
  earlier_requests_better_ = policy_->ShouldPreferEarlierRequests();
  current_conditions_.reset(new DeviceConditions(*device_conditions));
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(base::Bind(&RequestPicker::GetRequestResultCallback,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 disabled_requests));
}

// When we get contents from the queue, use them to pick the next
// request to operate on (if any).
void RequestPicker::GetRequestResultCallback(
    const std::set<int64_t>& disabled_requests,
    RequestQueue::GetRequestsResult,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away.
  if (requests.size() == 0) {
    not_picked_callback_.Run(false);
    return;
  }

  // Get the expired requests to be removed from the queue, and the valid ones
  // from which to pick the next request.
  std::vector<std::unique_ptr<SavePageRequest>> valid_requests;
  std::vector<std::unique_ptr<SavePageRequest>> expired_requests;
  SplitRequests(std::move(requests), &valid_requests, &expired_requests);
  std::vector<int64_t> expired_request_ids;
  for (const auto& request : expired_requests)
    expired_request_ids.push_back(request->request_id());

  queue_->RemoveRequests(expired_request_ids,
                         base::Bind(&RequestPicker::OnRequestExpired,
                                    weak_ptr_factory_.GetWeakPtr()));

  // Pick the most deserving request for our conditions.
  const SavePageRequest* picked_request = nullptr;

  RequestCompareFunction comparator = nullptr;

  // Choose which comparison function to use based on policy.
  if (policy_->RetryCountIsMoreImportantThanRecency())
    comparator = &RequestPicker::RetryCountFirstCompareFunction;
  else
    comparator = &RequestPicker::RecencyFirstCompareFunction;

  // Iterate once through the requests, keeping track of best candidate.
  bool non_user_requested_tasks_remaining = false;
  for (unsigned i = 0; i < valid_requests.size(); ++i) {
    // If the  request is on the disabled list, skip it.
    auto search = disabled_requests.find(valid_requests[i]->request_id());
    if (search != disabled_requests.end()) {
      continue;
    }
    if (!valid_requests[i]->user_requested())
      non_user_requested_tasks_remaining = true;
    if (!RequestConditionsSatisfied(valid_requests[i].get()))
      continue;
    if (IsNewRequestBetter(picked_request, valid_requests[i].get(), comparator))
      picked_request = valid_requests[i].get();
  }

  // If we have a best request to try next, get the request coodinator to
  // start it.  Otherwise return that we have no candidates.
  if (picked_request != nullptr) {
    picked_callback_.Run(*picked_request);
  } else {
    not_picked_callback_.Run(non_user_requested_tasks_remaining);
  }
}

// Filter out requests that don't meet the current conditions.  For instance, if
// this is a predictive request, and we are not on WiFi, it should be ignored
// this round.
bool RequestPicker::RequestConditionsSatisfied(const SavePageRequest* request) {
  // If the user did not request the page directly, make sure we are connected
  // to power and have WiFi and sufficient battery remaining before we take this
  // request.

  if (!current_conditions_->IsPowerConnected() &&
      policy_->PowerRequired(request->user_requested())) {
    return false;
  }

  if (current_conditions_->GetNetConnectionType() !=
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI &&
      policy_->UnmeteredNetworkRequired(request->user_requested())) {
    return false;
  }

  if (current_conditions_->GetBatteryPercentage() <
      policy_->BatteryPercentageRequired(request->user_requested())) {
    return false;
  }

  // If we have already started this page the max number of times, it is not
  // eligible to try again.
  if (request->started_attempt_count() >= policy_->GetMaxStartedTries())
    return false;

  // If we have already completed trying this page the max number of times, it
  // is not eligible to try again.
  if (request->completed_attempt_count() >= policy_->GetMaxCompletedTries())
    return false;

  // If the request is paused, do not consider it.
  if (request->request_state() == SavePageRequest::RequestState::PAUSED)
    return false;

  // If the request is expired, do not consider it.
  base::TimeDelta requestAge = base::Time::Now() - request->creation_time();
  if (requestAge >
      base::TimeDelta::FromSeconds(
          policy_->GetRequestExpirationTimeInSeconds()))
    return false;

  // If this request is not active yet, return false.
  // TODO(petewil): If the only reason we return nothing to do is that we have
  // inactive requests, we still want to try again later after their activation
  // time elapses, we shouldn't take ourselves completely off the scheduler.
  if (request->activation_time() > base::Time::Now())
    return false;

  return true;
}

// Look at policies to decide which requests to prefer.
bool RequestPicker::IsNewRequestBetter(const SavePageRequest* oldRequest,
                                       const SavePageRequest* newRequest,
                                       RequestCompareFunction comparator) {

  // If there is no old request, the new one is better.
  if (oldRequest == nullptr)
    return true;

  // User requested pages get priority.
  if (newRequest->user_requested() && !oldRequest->user_requested())
    return true;

  // Otherwise, use the comparison function for the current policy, which
  // returns true if the older request is better.
  return !(CALL_MEMBER_FUNCTION(this, comparator)(oldRequest, newRequest));
}

// Compare the results, checking request count before recency.  Returns true if
// left hand side is better, false otherwise.
bool RequestPicker::RetryCountFirstCompareFunction(
    const SavePageRequest* left, const SavePageRequest* right) {
  // Check the attempt count.
  int result = CompareRetryCount(left, right);

  if (result != 0)
    return (result > 0);

  // If we get here, the attempt counts were the same, so check recency.
  result = CompareCreationTime(left, right);

  return (result > 0);
}

// Compare the results, checking recency before request count. Returns true if
// left hand side is better, false otherwise.
bool RequestPicker::RecencyFirstCompareFunction(
    const SavePageRequest* left, const SavePageRequest* right) {
  // Check the recency.
  int result = CompareCreationTime(left, right);

  if (result != 0)
    return (result > 0);

  // If we get here, the recency was the same, so check the attempt count.
  result = CompareRetryCount(left, right);

  return (result > 0);
}

// Compare left and right side, returning 1 if the left side is better
// (preferred by policy), 0 if the same, and -1 if the right side is better.
int RequestPicker::CompareRetryCount(
    const SavePageRequest* left, const SavePageRequest* right) {
  // Check the attempt count.
  int result = signum(left->completed_attempt_count() -
                      right->completed_attempt_count());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (fewer_retries_better_)
    result *= -1;

  return result;
}

// Compare left and right side, returning 1 if the left side is better
// (preferred by policy), 0 if the same, and -1 if the right side is better.
int RequestPicker::CompareCreationTime(
    const SavePageRequest* left, const SavePageRequest* right) {
  // Check the recency.
  base::TimeDelta difference = left->creation_time() - right->creation_time();
  int result = signum(difference.InMilliseconds());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (earlier_requests_better_)
    result *= -1;

  return result;
}

void RequestPicker::SplitRequests(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    std::vector<std::unique_ptr<SavePageRequest>>* valid_requests,
    std::vector<std::unique_ptr<SavePageRequest>>* expired_requests) {
  for (auto& request : requests) {
    if (base::Time::Now() - request->creation_time() >=
        base::TimeDelta::FromSeconds(kRequestExpirationTimeInSeconds)) {
      expired_requests->push_back(std::move(request));
    } else {
      valid_requests->push_back(std::move(request));
    }
  }
}

// Callback used after expired requests are deleted from the queue and notifies
// the coordinator.
void RequestPicker::OnRequestExpired(
    std::unique_ptr<UpdateRequestsResult> result) {
  const RequestCoordinator::BackgroundSavePageResult save_page_result(
      RequestCoordinator::BackgroundSavePageResult::EXPIRED);
  for (const auto& request : result->updated_items) {
    event_logger_->RecordDroppedSavePageRequest(
        request.client_id().name_space, save_page_result, request.request_id());
    notifier_->NotifyCompleted(request, save_page_result);
  }
}

}  // namespace offline_pages
