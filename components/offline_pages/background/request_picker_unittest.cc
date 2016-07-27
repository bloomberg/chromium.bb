// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_picker.h"

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// Data for request 1.
const int64_t kRequestId1 = 17;
const GURL kUrl1("https://google.com");
const ClientId kClientId1("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 42;
const GURL kUrl2("http://nytimes.com");
const ClientId kClientId2("bookmark", "5678");
const bool kUserRequested = true;
const int kAttemptCount = 1;
const int kTwoWeeksInSeconds = 60 * 60 * 24 * 7 * 2;

// Constants for policy values - These settings represent the default values.
const bool kPreferUntried = false;
const bool kPreferEarlier = true;
const bool kPreferRetryCount = true;
}  // namespace

class RequestPickerTest : public testing::Test {
 public:
  RequestPickerTest();

  ~RequestPickerTest() override;

  void SetUp() override;

  void PumpLoop();

  void AddRequestDone(RequestQueue::AddRequestResult result,
                      const SavePageRequest& request);

  void RequestPicked(const SavePageRequest& request);

  void RequestQueueEmpty();

  void QueueRequestsAndChooseOne(const SavePageRequest& request1,
                                 const SavePageRequest& request2);

 protected:
  // The request queue is simple enough we will use a real queue with a memory
  // store instead of a stub.
  std::unique_ptr<RequestQueue> queue_;
  std::unique_ptr<RequestPicker> picker_;
  std::unique_ptr<SavePageRequest> last_picked_;
  std::unique_ptr<OfflinerPolicy> policy_;
  bool request_queue_empty_called_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestPickerTest::RequestPickerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestPickerTest::~RequestPickerTest() {}

void RequestPickerTest::SetUp() {
  std::unique_ptr<RequestQueueInMemoryStore> store(
      new RequestQueueInMemoryStore());
  queue_.reset(new RequestQueue(std::move(store)));
  policy_.reset(new OfflinerPolicy());
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));
  request_queue_empty_called_ = false;
}

void RequestPickerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestPickerTest::AddRequestDone(RequestQueue::AddRequestResult result,
                                       const SavePageRequest& request) {}

void RequestPickerTest::RequestPicked(const SavePageRequest& request) {
  last_picked_.reset(new SavePageRequest(request));
}

void RequestPickerTest::RequestQueueEmpty() {
  request_queue_empty_called_ = true;
}

// Test helper to queue the two given requests and then pick one of them per
// configured policy.
void RequestPickerTest::QueueRequestsAndChooseOne(
    const SavePageRequest& request1, const SavePageRequest& request2) {
  DeviceConditions conditions;
  // Add test requests on the Queue.
  queue_->AddRequest(request1, base::Bind(&RequestPickerTest::AddRequestDone,
                                          base::Unretained(this)));
  queue_->AddRequest(request2, base::Bind(&RequestPickerTest::AddRequestDone,
                                          base::Unretained(this)));

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();

  // Call the method under test.
  picker_->ChooseNextRequest(
      base::Bind(&RequestPickerTest::RequestPicked, base::Unretained(this)),
      base::Bind(&RequestPickerTest::RequestQueueEmpty, base::Unretained(this)),
      &conditions);

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();
}

TEST_F(RequestPickerTest, PickFromEmptyQueue) {
  DeviceConditions conditions;
  picker_->ChooseNextRequest(
      base::Bind(&RequestPickerTest::RequestPicked, base::Unretained(this)),
      base::Bind(&RequestPickerTest::RequestQueueEmpty, base::Unretained(this)),
      &conditions);

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "QueueEmpty"
  // callback.
  PumpLoop();

  EXPECT_TRUE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseRequestWithHigherRetryCount) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(
      kRequestId1, kUrl1, kClientId1, creation_time, kUserRequested);
  SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, creation_time, kUserRequested);
  request2.set_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseRequestWithSameRetryCountButEarlier) {
  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseEarlierRequest) {
  // We need a custom policy object prefering recency to retry count.
  policy_.reset(
      new OfflinerPolicy(kPreferUntried, kPreferEarlier, !kPreferRetryCount));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);
  request2.set_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseSameTimeRequestWithHigherRetryCount) {
  // We need a custom policy object preferring recency to retry count.
  policy_.reset(
      new OfflinerPolicy(kPreferUntried, kPreferEarlier, !kPreferRetryCount));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseRequestWithLowerRetryCount) {
  // We need a custom policy object preferring lower retry count.
  policy_.reset(
      new OfflinerPolicy(!kPreferUntried, kPreferEarlier, kPreferRetryCount));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseLaterRequest) {
  // We need a custom policy preferring recency over retry, and later requests.
  policy_.reset(
      new OfflinerPolicy(kPreferUntried, !kPreferEarlier, !kPreferRetryCount));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get()));

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, ChooseUnexpiredRequest) {
  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(kTwoWeeksInSeconds);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

}  // namespace offline_pages
