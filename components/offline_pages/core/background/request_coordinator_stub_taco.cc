// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"

#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/background/scheduler.h"
#include "components/offline_pages/core/background/scheduler_stub.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/offline_pages_ukm_reporter_stub.h"
#include "services/network/test/test_network_quality_tracker.h"

namespace offline_pages {

RequestCoordinatorStubTaco::RequestCoordinatorStubTaco() {
  policy_ = std::make_unique<OfflinerPolicy>();
  queue_ =
      std::make_unique<RequestQueue>(std::make_unique<TestRequestQueueStore>());
  offliner_ = std::make_unique<OfflinerStub>();
  scheduler_ = std::make_unique<SchedulerStub>();
  network_quality_tracker_ =
      std::make_unique<network::TestNetworkQualityTracker>();
  ukm_reporter_ = std::make_unique<OfflinePagesUkmReporterStub>();
}

RequestCoordinatorStubTaco::~RequestCoordinatorStubTaco() {
}

void RequestCoordinatorStubTaco::SetOfflinerPolicy(
    std::unique_ptr<OfflinerPolicy> policy) {
  CHECK(!request_coordinator_);
  policy_ = std::move(policy);
}

void RequestCoordinatorStubTaco::SetRequestQueueStore(
    std::unique_ptr<RequestQueueStore> store) {
  CHECK(!request_coordinator_ && !queue_overridden_);
  store_overridden_ = true;
  queue_ = std::make_unique<RequestQueue>(std::move(store));
}

void RequestCoordinatorStubTaco::SetRequestQueue(
    std::unique_ptr<RequestQueue> queue) {
  CHECK(!request_coordinator_ && !store_overridden_);
  queue_overridden_ = true;
  queue_ = std::move(queue);
}

void RequestCoordinatorStubTaco::SetOffliner(
    std::unique_ptr<Offliner> offliner) {
  CHECK(!request_coordinator_);
  offliner_ = std::move(offliner);
}

void RequestCoordinatorStubTaco::SetScheduler(
    std::unique_ptr<Scheduler> scheduler) {
  CHECK(!request_coordinator_);
  scheduler_ = std::move(scheduler);
}

void RequestCoordinatorStubTaco::SetNetworkQualityProvider(
    std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker) {
  CHECK(!request_coordinator_);
  network_quality_tracker_ = std::move(network_quality_tracker);
}

void RequestCoordinatorStubTaco::SetOfflinePagesUkmReporter(
    std::unique_ptr<offline_pages::OfflinePagesUkmReporter> ukm_reporter) {
  ukm_reporter_ = std::move(ukm_reporter);
}

void RequestCoordinatorStubTaco::CreateRequestCoordinator() {
  request_coordinator_ = std::make_unique<RequestCoordinator>(
      std::move(policy_), std::move(offliner_), std::move(queue_),
      std::move(scheduler_), network_quality_tracker_.get(),
      std::move(ukm_reporter_));
}

RequestCoordinator* RequestCoordinatorStubTaco::request_coordinator() {
  CHECK(request_coordinator_);
  return request_coordinator_.get();
}
}  // namespace offline_page
