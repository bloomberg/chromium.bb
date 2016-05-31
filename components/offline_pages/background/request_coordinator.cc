// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_picker.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/background/scheduler.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

RequestCoordinator::RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                                       std::unique_ptr<OfflinerFactory> factory,
                                       std::unique_ptr<RequestQueue> queue,
                                       std::unique_ptr<Scheduler> scheduler)
    : policy_(std::move(policy)),
      factory_(std::move(factory)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  picker_.reset(new RequestPicker(queue_.get()));
}

RequestCoordinator::~RequestCoordinator() {}

bool RequestCoordinator::SavePageLater(
    const GURL& url, const ClientId& client_id) {
  DVLOG(2) << "URL is " << url << " " << __FUNCTION__;

  // TODO(petewil): We need a robust scheme for allocating new IDs.
  static int64_t id = 0;

  // Build a SavePageRequest.
  // TODO(petewil): Use something like base::Clock to help in testing.
  offline_pages::SavePageRequest request(
      id++, url, client_id, base::Time::Now());

  // Put the request on the request queue.
  queue_->AddRequest(request,
                     base::Bind(&RequestCoordinator::AddRequestResultCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  // TODO(petewil): Do I need to persist the request in case the add fails?

  // TODO(petewil): Make a new chromium command line switch to send the request
  // immediately for testing.  It should call SendRequestToOffliner()

  return true;
}

void RequestCoordinator::AddRequestResultCallback(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {

  // Inform the scheduler that we have an outstanding task.
  // TODO(petewil): Define proper TriggerConditions and set them.
  Scheduler::TriggerCondition conditions;
  scheduler_->Schedule(conditions);
}

void RequestCoordinator::RequestPicked(const SavePageRequest& request) {
  // Send the request on to the offliner.
  SendRequestToOffliner(request);
}

void RequestCoordinator::RequestQueueEmpty() {
  // TODO(petewil): return to the BackgroundScheduler by calling
  // ProcessingDoneCallback
}

bool RequestCoordinator::StartProcessing(
    const ProcessingDoneCallback& callback) {
  // TODO(petewil): Check existing conditions (should be passed down from
  // BackgroundTask)

  // Choose a request to process that meets the available conditions.
  // This is an async call, and returns right away.
  picker_->ChooseNextRequest(
      base::Bind(&RequestCoordinator::RequestPicked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&RequestCoordinator::RequestQueueEmpty,
                 weak_ptr_factory_.GetWeakPtr()));
  return false;
}

void RequestCoordinator::StopProcessing() {
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  // TODO(petewil): When we have multiple offliners, we need to pick one.
  Offliner* offliner = factory_->GetOffliner(policy_.get());
  if (!offliner) {
    DVLOG(0) << "Unable to create Offliner. "
             << "Cannot background offline page.";
    return;
  }

  // Start the load and save process in the offliner (Async).
  offliner->LoadAndSave(request,
                        base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED) << ", "
           << __FUNCTION__;
  last_offlining_status_ = status;

  // TODO(petewil): Check time budget.  Start a request if we have time, return
  // to the scheduler if we are out of time.

  // TODO(petewil): If the request succeeded, remove it from the Queue.
}

}  // namespace offline_pages
