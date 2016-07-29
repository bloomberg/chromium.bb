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

#define CALL_MEMBER_FUNCTION(object,ptrToMember)  ((object)->*(ptrToMember))
}  // namespace

namespace offline_pages {

RequestPicker::RequestPicker(
    RequestQueue* requestQueue, OfflinerPolicy* policy)
    : queue_(requestQueue),
      policy_(policy),
      fewer_retries_better_(false),
      earlier_requests_better_(false),
      weak_ptr_factory_(this) {}

RequestPicker::~RequestPicker() {}

// Entry point for the async operation to choose the next request.
void RequestPicker::ChooseNextRequest(
    RequestCoordinator::RequestPickedCallback picked_callback,
    RequestCoordinator::RequestQueueEmptyCallback empty_callback,
    DeviceConditions* device_conditions) {
  picked_callback_ = picked_callback;
  empty_callback_ = empty_callback;
  fewer_retries_better_ = policy_->ShouldPreferUntriedRequests();
  earlier_requests_better_ = policy_->ShouldPreferEarlierRequests();
  current_conditions_.reset(new DeviceConditions(*device_conditions));
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(base::Bind(&RequestPicker::GetRequestResultCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
}

// When we get contents from the queue, use them to pick the next
// request to operate on (if any).
void RequestPicker::GetRequestResultCallback(
    RequestQueue::GetRequestsResult,
    const std::vector<SavePageRequest>& requests) {
  // If there is nothing to do, return right away.
  if (requests.size() == 0) {
    empty_callback_.Run();
    return;
  }

  // Pick the most deserving request for our conditions.
  const SavePageRequest* picked_request = nullptr;

  RequestCompareFunction comparator = nullptr;

  // Choose which comparison function to use based on policy.
  if (policy_->RetryCountIsMoreImportantThanRecency())
    comparator = &RequestPicker::RetryCountFirstCompareFunction;
  else
    comparator = &RequestPicker::RecencyFirstCompareFunction;

  // Iterate once through the requests, keeping track of best candidate.
  for (unsigned i = 0; i < requests.size(); ++i) {
    if (!RequestConditionsSatisfied(requests[i]))
      continue;
    if (IsNewRequestBetter(picked_request, &(requests[i]), comparator))
      picked_request = &(requests[i]);
  }

  // If we have a best request to try next, get the request coodinator to
  // start it.  Otherwise return that we have no candidates.
  if (picked_request != nullptr) {
    picked_callback_.Run(*picked_request);
  } else {
    empty_callback_.Run();
  }
}

// Filter out requests that don't meet the current conditions.  For instance, if
// this is a predictive request, and we are not on WiFi, it should be ignored
// this round.
bool RequestPicker::RequestConditionsSatisfied(const SavePageRequest& request) {
  // If the user did not request the page directly, make sure we are connected
  // to power and have WiFi and sufficient battery remaining before we take this
  // reqeust.
  // TODO(petewil): We may later want to configure whether to allow 2G for non
  // user_requested items, add that to policy.
  if (!request.user_requested()) {
    if (!current_conditions_->IsPowerConnected())
      return false;

    if (current_conditions_->GetNetConnectionType() !=
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI) {
      return false;
    }

    if (current_conditions_->GetBatteryPercentage() <
        policy_->GetMinimumBatteryPercentageForNonUserRequestOfflining()) {
      return false;
    }
  }

  // If we have already tried this page the max number of times, it is not
  // eligible to try again.
  // TODO(petewil): Instead, we should have code to remove the page from the
  // queue after the last retry.
  if (request.attempt_count() > policy_->GetMaxTries())
    return false;

  // If the request is expired, do not consider it.
  // TODO(petewil): We need to remove this from the queue.
  base::TimeDelta requestAge = base::Time::Now() - request.creation_time();
  if (requestAge >
      base::TimeDelta::FromSeconds(
          policy_->GetRequestExpirationTimeInSeconds()))
    return false;

  // If this request is not active yet, return false.
  // TODO(petewil): If the only reason we return nothing to do is that we have
  // inactive requests, we still want to try again later after their activation
  // time elapses, we shouldn't take ourselves completely off the scheduler.
  if (request.activation_time() > base::Time::Now())
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
  int result = signum(left->attempt_count() - right->attempt_count());

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

}  // namespace offline_pages
