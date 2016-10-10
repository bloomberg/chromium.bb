// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/request_coordinator_event_logger.h"
#include "components/offline_pages/background/request_queue.h"

namespace offline_pages {

class RequestNotifier;

typedef bool (RequestPicker::*RequestCompareFunction)(
    const SavePageRequest* left, const SavePageRequest* right);

class RequestPicker {
 public:
  RequestPicker(RequestQueue* requestQueue,
                OfflinerPolicy* policy,
                RequestNotifier* notifier,
                RequestCoordinatorEventLogger* event_logger);

  ~RequestPicker();

  // Choose which request we should process next based on the current
  // conditions, and call back to the RequestCoordinator when we have one.
  void ChooseNextRequest(
      RequestCoordinator::RequestPickedCallback picked_callback,
      RequestCoordinator::RequestNotPickedCallback not_picked_callback,
      DeviceConditions* device_conditions,
      const std::set<int64_t>& disabled_requests);

 private:
  // Callback for the GetRequest results to be delivered.
  void GetRequestResultCallback(
      const std::set<int64_t>& disabled_requests,
      RequestQueue::GetRequestsResult result,
      std::vector<std::unique_ptr<SavePageRequest>> results);

  // Filter out requests that don't meet the current conditions.  For instance,
  // if this is a predictive request, and we are not on WiFi, it should be
  // ignored this round.
  bool RequestConditionsSatisfied(const SavePageRequest* request);

  // Using policies, decide if the new request is preferable to the best we have
  // so far.
  bool IsNewRequestBetter(const SavePageRequest* oldRequest,
                          const SavePageRequest* newRequest,
                          RequestCompareFunction comparator);

  // Is the new request preferable from the retry count first standpoint?
  bool RetryCountFirstCompareFunction(const SavePageRequest* left,
                                      const SavePageRequest* right);

  // Is the new request better from the recency first standpoint?
  bool RecencyFirstCompareFunction(const SavePageRequest* left,
                                   const SavePageRequest* right);

  // Does the new request have better retry count?
  int CompareRetryCount(const SavePageRequest* left,
                        const SavePageRequest* right);

  // Does the new request have better creation time?
  int CompareCreationTime(const SavePageRequest* left,
                          const SavePageRequest* right);

  // Split all requests into expired ones and still valid ones.  Takes ownership
  // of the requests, and moves them into either valid or expired requests.
  void SplitRequests(
      std::vector<std::unique_ptr<SavePageRequest>> requests,
      std::vector<std::unique_ptr<SavePageRequest>>* valid_requests,
      std::vector<std::unique_ptr<SavePageRequest>>* expired_requests);

  // Callback used after requests get expired.
  void OnRequestExpired(std::unique_ptr<UpdateRequestsResult> result);

  // Unowned pointer to the request queue.
  RequestQueue* queue_;
  // Unowned pointer to the policy object.
  OfflinerPolicy* policy_;
  // Unowned pointer to the request coordinator.
  RequestNotifier* notifier_;
  // Unowned pointer to the event logger.
  RequestCoordinatorEventLogger* event_logger_;
  // Current conditions on the device
  std::unique_ptr<DeviceConditions> current_conditions_;
  // True if we prefer less-tried requests
  bool fewer_retries_better_;
  // True if we prefer requests submitted more recently
  bool earlier_requests_better_;
  // Callback for when we are done picking a request to do next.
  RequestCoordinator::RequestPickedCallback picked_callback_;
  // Callback for when there are no more reqeusts to pick.
  RequestCoordinator::RequestNotPickedCallback not_picked_callback_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestPicker> weak_ptr_factory_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_
