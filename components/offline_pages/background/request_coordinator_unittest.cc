// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/background/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const GURL kUrl("http://universe.com/everything");
const ClientId kClientId("bookmark", "42");
}  // namespace

class SchedulerStub : public Scheduler {
 public:
  SchedulerStub() : schedule_called_(false), unschedule_called_(false) {}

  void Schedule(const TriggerCondition& trigger_condition) override {
    schedule_called_ = true;
  }

  // Unschedules the currently scheduled task, if any.
  void Unschedule() override {
    unschedule_called_ = true;
  }

  bool schedule_called() const { return schedule_called_; }

  bool unschedule_called() const { return unschedule_called_; }

 private:
  bool schedule_called_;
  bool unschedule_called_;
};

class OfflinerStub : public Offliner {
  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override {
    // Post the callback on the run loop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, request, Offliner::SAVED));
    return true;
  }

  // Clears the currently processing request, if any.
  void Cancel() override {}
};

class OfflinerFactoryStub : public OfflinerFactory {
 public:
  Offliner* GetOffliner(const OfflinerPolicy* policy) override {
    offliner_.reset(new OfflinerStub());
    return offliner_.get();
  }

 private:
  std::unique_ptr<Offliner> offliner_;
};

class RequestCoordinatorTest
    : public testing::Test {
 public:
  RequestCoordinatorTest();
  ~RequestCoordinatorTest() override;

  void SetUp() override;

  void PumpLoop();

  RequestCoordinator* coordinator() {
    return coordinator_.get();
  }

  // Empty callback function
  void EmptyCallbackFunction() {
  }

  // Callback for getting requests.
  void GetRequestsDone(RequestQueue::GetRequestsResult result,
                       const std::vector<SavePageRequest>& requests);

  RequestQueue::GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

 private:
  RequestQueue::GetRequestsResult last_get_requests_result_;
  std::vector<SavePageRequest> last_requests_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<RequestCoordinator> coordinator_;
};

RequestCoordinatorTest::RequestCoordinatorTest()
    : last_get_requests_result_(RequestQueue::GetRequestsResult::kStoreFailure),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestCoordinatorTest::~RequestCoordinatorTest() {}

void RequestCoordinatorTest::SetUp() {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<OfflinerFactory> factory(new OfflinerFactoryStub());
  std::unique_ptr<RequestQueueInMemoryStore>
      store(new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  std::unique_ptr<Scheduler> scheduler_stub(new SchedulerStub());
  coordinator_.reset(new RequestCoordinator(
      std::move(policy), std::move(factory), std::move(queue),
      std::move(scheduler_stub)));
}

void RequestCoordinatorTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestCoordinatorTest::GetRequestsDone(
    RequestQueue::GetRequestsResult result,
    const std::vector<SavePageRequest>& requests) {
  last_get_requests_result_ = result;
  last_requests_ = requests;
}

TEST_F(RequestCoordinatorTest, StartProcessingWithNoRequests) {
  RequestCoordinator::ProcessingDoneCallback callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_FALSE(coordinator()->StartProcessing(callback));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl, kClientId));

  // Expect that a request got placed on the queue.
  coordinator()->GetQueue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  PumpLoop();

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl, last_requests()[0].url());
  EXPECT_EQ(kClientId, last_requests()[0].client_id());

  // Expect that the scheduler got notified.
  SchedulerStub* scheduler_stub = reinterpret_cast<SchedulerStub*>(
      coordinator()->GetSchedulerForTesting());
  EXPECT_TRUE(scheduler_stub->schedule_called());

  // Check that the offliner callback got a response.
  EXPECT_EQ(Offliner::SAVED, coordinator()->last_offlining_status());

  // TODO(petewil): Expect that the scheduler got notified.
}

}  // namespace offline_pages
