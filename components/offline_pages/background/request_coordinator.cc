// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "base/bind.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
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
      last_offlining_status_(Offliner::UNKNOWN) {
  DCHECK(policy_ != nullptr);
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
                                AsWeakPtr()));
  // TODO: Do I need to persist the request in case the add fails?

  // TODO(petewil): Eventually we will wait for the StartProcessing callback,
  // but for now just kick start the request so we can test the wiring.
  SendRequestToOffliner(request);

  return true;
}

void RequestCoordinator::AddRequestResultCallback(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {
  DVLOG(2) << __FUNCTION__;

  // Inform the scheduler that we have an outstanding task.
  // TODO(petewil): Define proper TriggerConditions and set them.
  Scheduler::TriggerCondition conditions;
  scheduler_->Schedule(conditions);
}

bool RequestCoordinator::StartProcessing(
    const ProcessingDoneCallback& callback) {
  return false;
}

void RequestCoordinator::StopProcessing() {
}

void RequestCoordinator::SendRequestToOffliner(SavePageRequest& request) {
  // TODO(petewil): When we have multiple offliners, we need to pick one.
  Offliner* offliner = factory_->GetOffliner(policy_.get());
  if (!offliner) {
    LOG(ERROR) << "Unable to create Prerendering Offliner. "
               << "Cannot background offline page.";
    return;
  }

  // Start the load and save process in the prerenderer (Async).
  offliner->LoadAndSave(
      request,
      base::Bind(&RequestCoordinator::OfflinerDoneCallback, AsWeakPtr()));
}

void RequestCoordinator::OfflinerDoneCallback(
    const SavePageRequest& request,
    Offliner::CompletionStatus status) {
  DVLOG(2) << "prerenderer finished, status " << status << ", " << __FUNCTION__;
  last_offlining_status_ = status;
}

}  // namespace offline_pages
