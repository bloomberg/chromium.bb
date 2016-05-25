// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/request_queue.h"
#include "url/gurl.h"

namespace offline_pages {

struct ClientId;
class OfflinerPolicy;
class OfflinerFactory;
class Offliner;
class SavePageRequest;
class Scheduler;

// Coordinates queueing and processing save page later requests.
class RequestCoordinator :
    public KeyedService,
    public base::SupportsWeakPtr<RequestCoordinator>  {
 public:
  // Callback to report when the processing of a triggered task is complete.
  typedef base::Callback<void()> ProcessingDoneCallback;

  RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                     std::unique_ptr<OfflinerFactory> factory,
                     std::unique_ptr<RequestQueue> queue,
                     std::unique_ptr<Scheduler> scheduler);

  ~RequestCoordinator() override;

  // Queues |request| to later load and save when system conditions allow.
  // Returns true if the page could be queued successfully.
  bool SavePageLater(const GURL& url, const ClientId& client_id);

  // Starts processing of one or more queued save page later requests.
  // Returns whether processing was started and that caller should expect
  // a callback. If processing was already active, returns false.
  bool StartProcessing(const ProcessingDoneCallback& callback);

  // Stops the current request processing if active. This is a way for
  // caller to abort processing; otherwise, processing will complete on
  // its own. In either case, the callback will be called when processing
  // is stopped or complete.
  void StopProcessing();

  // Returns the request queue used for requests.  Coordinator keeps ownership.
  RequestQueue* GetQueue() { return queue_.get(); }

  Scheduler* GetSchedulerForTesting() { return scheduler_.get(); }

  // Returns the status of the most recent offlining.
  Offliner::CompletionStatus last_offlining_status() {
    return last_offlining_status_;
  }

 private:
  void AddRequestResultCallback(RequestQueue::AddRequestResult result,
                                const SavePageRequest& request);

  void SendRequestToOffliner(SavePageRequest& request);

  void OfflinerDoneCallback(const SavePageRequest& request,
                            Offliner::CompletionStatus status);

  // RequestCoordinator takes over ownership of the policy
  std::unique_ptr<OfflinerPolicy> policy_;
  // OfflinerFactory.  Used to create offline pages. Owned.
  std::unique_ptr<OfflinerFactory> factory_;
  // RequestQueue.  Used to store incoming requests. Owned.
  std::unique_ptr<RequestQueue> queue_;
  // Scheduler. Used to request a callback when network is available.  Owned.
  std::unique_ptr<Scheduler> scheduler_;
  // Status of the most recent offlining.
  Offliner::CompletionStatus last_offlining_status_;

  DISALLOW_COPY_AND_ASSIGN(RequestCoordinator);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
