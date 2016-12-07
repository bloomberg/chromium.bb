// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/pick_request_task.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/offliner_policy_utils.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace {
template <typename T>
int signum(T t) {
  return (T(0) < t) - (t < T(0));
}

bool kCleanupNeeded = true;
bool kNonUserRequestsFound = true;

#define CALL_MEMBER_FUNCTION(object, ptrToMember) ((object)->*(ptrToMember))
}  // namespace

namespace offline_pages {

PickRequestTask::PickRequestTask(RequestQueueStore* store,
                                 OfflinerPolicy* policy,
                                 RequestPickedCallback picked_callback,
                                 RequestNotPickedCallback not_picked_callback,
                                 RequestCountCallback request_count_callback,
                                 DeviceConditions& device_conditions,
                                 const std::set<int64_t>& disabled_requests)
    : store_(store),
      policy_(policy),
      picked_callback_(picked_callback),
      not_picked_callback_(not_picked_callback),
      request_count_callback_(request_count_callback),
      disabled_requests_(disabled_requests),
      weak_ptr_factory_(this) {
  device_conditions_.reset(new DeviceConditions(device_conditions));
}

PickRequestTask::~PickRequestTask() {}

void PickRequestTask::Run() {
  GetRequests();
}

void PickRequestTask::GetRequests() {
  // Get all the requests from the queue, we will classify them in the callback.
  store_->GetRequests(
      base::Bind(&PickRequestTask::Choose, weak_ptr_factory_.GetWeakPtr()));
}

void PickRequestTask::Choose(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away.
  if (requests.empty()) {
    request_count_callback_.Run(requests.size(), 0);
    not_picked_callback_.Run(!kNonUserRequestsFound, !kCleanupNeeded);
    TaskComplete();
    return;
  }

  // Pick the most deserving request for our conditions.
  const SavePageRequest* picked_request = nullptr;

  RequestCompareFunction comparator = nullptr;

  // Choose which comparison function to use based on policy.
  if (policy_->RetryCountIsMoreImportantThanRecency())
    comparator = &PickRequestTask::RetryCountFirstCompareFunction;
  else
    comparator = &PickRequestTask::RecencyFirstCompareFunction;

  // TODO(petewil): Consider replacing this bool with a better named enum.
  bool non_user_requested_tasks_remaining = false;
  bool cleanup_needed = false;

  size_t available_request_count = 0;

  // Iterate once through the requests, keeping track of best candidate.
  for (unsigned i = 0; i < requests.size(); ++i) {
    // If the request is expired or has exceeded the retry count, skip it.
    if (OfflinerPolicyUtils::CheckRequestExpirationStatus(requests[i].get(),
                                                          policy_) !=
        OfflinerPolicyUtils::RequestExpirationStatus::VALID) {
      cleanup_needed = true;
      continue;
    }

    // If the  request is on the disabled list, skip it.
    auto search = disabled_requests_.find(requests[i]->request_id());
    if (search != disabled_requests_.end())
      continue;

    // If there are non-user-requested tasks remaining, we need to make sure
    // that they are scheduled after we run out of user requested tasks. Here we
    // detect if any exist. If we don't find any user-requested tasks, we will
    // inform the "not_picked_callback_" that it needs to schedule a task for
    // non-user-requested items, which have different network and power needs.
    if (!requests[i]->user_requested())
      non_user_requested_tasks_remaining = true;
    if (requests[i]->request_state() ==
        SavePageRequest::RequestState::AVAILABLE) {
      available_request_count++;
    }
    if (!RequestConditionsSatisfied(requests[i].get()))
      continue;
    if (IsNewRequestBetter(picked_request, requests[i].get(), comparator))
      picked_request = requests[i].get();
  }

  // Report the request queue counts.
  request_count_callback_.Run(requests.size(), available_request_count);

  // If we have a best request to try next, get the request coodinator to
  // start it.  Otherwise return that we have no candidates.
  if (picked_request != nullptr) {
    picked_callback_.Run(*picked_request, cleanup_needed);
  } else {
    not_picked_callback_.Run(non_user_requested_tasks_remaining,
                             cleanup_needed);
  }

  TaskComplete();
}

// Filter out requests that don't meet the current conditions.  For instance, if
// this is a predictive request, and we are not on WiFi, it should be ignored
// this round.
bool PickRequestTask::RequestConditionsSatisfied(
    const SavePageRequest* request) {
  // If the user did not request the page directly, make sure we are connected
  // to power and have WiFi and sufficient battery remaining before we take this
  // request.
  if (!device_conditions_->IsPowerConnected() &&
      policy_->PowerRequired(request->user_requested())) {
    return false;
  }

  if (device_conditions_->GetNetConnectionType() !=
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI &&
      policy_->UnmeteredNetworkRequired(request->user_requested())) {
    return false;
  }

  if (device_conditions_->GetBatteryPercentage() <
      policy_->BatteryPercentageRequired(request->user_requested())) {
    return false;
  }

  // If the request is paused, do not consider it.
  if (request->request_state() == SavePageRequest::RequestState::PAUSED)
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
bool PickRequestTask::IsNewRequestBetter(const SavePageRequest* oldRequest,
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
bool PickRequestTask::RetryCountFirstCompareFunction(
    const SavePageRequest* left,
    const SavePageRequest* right) {
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
bool PickRequestTask::RecencyFirstCompareFunction(
    const SavePageRequest* left,
    const SavePageRequest* right) {
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
int PickRequestTask::CompareRetryCount(const SavePageRequest* left,
                                       const SavePageRequest* right) {
  // Check the attempt count.
  int result = signum(left->completed_attempt_count() -
                      right->completed_attempt_count());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (policy_->ShouldPreferUntriedRequests())
    result *= -1;

  return result;
}

// Compare left and right side, returning 1 if the left side is better
// (preferred by policy), 0 if the same, and -1 if the right side is better.
int PickRequestTask::CompareCreationTime(const SavePageRequest* left,
                                         const SavePageRequest* right) {
  // Check the recency.
  base::TimeDelta difference = left->creation_time() - right->creation_time();
  int result = signum(difference.InMilliseconds());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (policy_->ShouldPreferEarlierRequests())
    result *= -1;

  return result;
}

}  // namespace offline_pages
