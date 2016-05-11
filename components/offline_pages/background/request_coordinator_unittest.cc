// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const GURL kUrl("http://universe.com/everything");
const ClientId kClientId("bookmark", "42");
}  // namespace

class RequestCoordinatorTest
    : public testing::Test {
 public:
  RequestCoordinatorTest();
  ~RequestCoordinatorTest() override;

  void SetUp() override;

  void PumpLoop();

  RequestCoordinator* getCoordinator() {
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
  std::unique_ptr<OfflinerFactory> factory;
  std::unique_ptr<RequestQueueInMemoryStore>
      store(new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  coordinator_.reset(new RequestCoordinator(
      std::move(policy), std::move(factory), std::move(queue)));
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
  EXPECT_FALSE(getCoordinator()->StartProcessing(callback));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  EXPECT_TRUE(getCoordinator()->SavePageLater(kUrl, kClientId));

  // Expect that a request got placed on the queue.
  getCoordinator()->GetQueue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));

  // Wait for callback to finish.
  PumpLoop();

  // Check the results are as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl, last_requests()[0].url());
  EXPECT_EQ(kClientId, last_requests()[0].client_id());

  // TODO(petewil): Expect that the scheduler got notified.
}

}  // namespace offline_pages
