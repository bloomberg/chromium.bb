// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_picker.h"

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_notifier.h"
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
const int kMaxStartedTries = 5;
const int kMaxCompletedTries = 1;

// Constants for policy values - These settings represent the default values.
const bool kPreferUntried = false;
const bool kPreferEarlier = true;
const bool kPreferRetryCount = true;

// Default request
const SavePageRequest kEmptyRequest(0UL,
                                    GURL(""),
                                    ClientId("", ""),
                                    base::Time(),
                                    true);
}  // namespace

class RequestNotifierStub : public RequestNotifier {
 public:
  RequestNotifierStub()
      : last_expired_request_(kEmptyRequest), total_expired_requests_(0) {}

  void NotifyAdded(const SavePageRequest& request) override {}
  void NotifyChanged(const SavePageRequest& request) override {}

  void NotifyCompleted(const SavePageRequest& request,
                       BackgroundSavePageResult status) override {
    last_expired_request_ = request;
    last_request_expiration_status_ = status;
    total_expired_requests_++;
  }

  const SavePageRequest& last_expired_request() {
    return last_expired_request_;
  }

  RequestCoordinator::BackgroundSavePageResult
  last_request_expiration_status() {
    return last_request_expiration_status_;
  }

  int32_t total_expired_requests() { return total_expired_requests_; }

 private:
  BackgroundSavePageResult last_request_expiration_status_;
  SavePageRequest last_expired_request_;
  int32_t total_expired_requests_;
};

class RequestPickerTest : public testing::Test {
 public:
  RequestPickerTest();

  ~RequestPickerTest() override;

  void SetUp() override;

  void PumpLoop();

  void AddRequestDone(RequestQueue::AddRequestResult result,
                      const SavePageRequest& request);

  void RequestPicked(const SavePageRequest& request);

  void RequestNotPicked(const bool non_user_requested_tasks_remaining);

  void QueueRequestsAndChooseOne(const SavePageRequest& request1,
                                 const SavePageRequest& request2);

  RequestNotifierStub* GetNotifier() { return notifier_.get(); }

 protected:
  // The request queue is simple enough we will use a real queue with a memory
  // store instead of a stub.
  std::unique_ptr<RequestQueue> queue_;
  std::unique_ptr<RequestPicker> picker_;
  std::unique_ptr<RequestNotifierStub> notifier_;
  std::unique_ptr<SavePageRequest> last_picked_;
  std::unique_ptr<OfflinerPolicy> policy_;
  RequestCoordinatorEventLogger event_logger_;
  bool request_queue_not_picked_called_;

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
  notifier_.reset(new RequestNotifierStub());
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));
  request_queue_not_picked_called_ = false;
}

void RequestPickerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestPickerTest::AddRequestDone(RequestQueue::AddRequestResult result,
                                       const SavePageRequest& request) {}

void RequestPickerTest::RequestPicked(const SavePageRequest& request) {
  last_picked_.reset(new SavePageRequest(request));
}

void RequestPickerTest::RequestNotPicked(
    const bool non_user_requested_tasks_remaining) {
  request_queue_not_picked_called_ = true;
}

// Test helper to queue the two given requests and then pick one of them per
// configured policy.
void RequestPickerTest::QueueRequestsAndChooseOne(
    const SavePageRequest& request1, const SavePageRequest& request2) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
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
      base::Bind(&RequestPickerTest::RequestNotPicked, base::Unretained(this)),
      &conditions, disabled_requests);

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();
}

TEST_F(RequestPickerTest, PickFromEmptyQueue) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
  picker_->ChooseNextRequest(
      base::Bind(&RequestPickerTest::RequestPicked, base::Unretained(this)),
      base::Bind(&RequestPickerTest::RequestNotPicked, base::Unretained(this)),
      &conditions, disabled_requests);

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "QueueEmpty"
  // callback.
  PumpLoop();

  EXPECT_TRUE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseRequestWithHigherRetryCount) {
  policy_.reset(new OfflinerPolicy(kPreferUntried, kPreferEarlier,
                                   kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries + 1));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(
      kRequestId1, kUrl1, kClientId1, creation_time, kUserRequested);
  SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
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
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseEarlierRequest) {
  // We need a custom policy object prefering recency to retry count.
  policy_.reset(new OfflinerPolicy(kPreferUntried, kPreferEarlier,
                                   !kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseSameTimeRequestWithHigherRetryCount) {
  // We need a custom policy object preferring recency to retry count.
  policy_.reset(new OfflinerPolicy(kPreferUntried, kPreferEarlier,
                                   !kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries + 1));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseRequestWithLowerRetryCount) {
  // We need a custom policy object preferring lower retry count.
  policy_.reset(new OfflinerPolicy(!kPreferUntried, kPreferEarlier,
                                   kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries + 1));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseLaterRequest) {
  // We need a custom policy preferring recency over retry, and later requests.
  policy_.reset(new OfflinerPolicy(kPreferUntried, !kPreferEarlier,
                                   !kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseNonExpiredRequest) {
  base::Time creation_time = base::Time::Now();
  base::Time expired_time =
      creation_time - base::TimeDelta::FromSeconds(
                          policy_->GetRequestExpirationTimeInSeconds() + 60);
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, expired_time,
                           kUserRequested);

  QueueRequestsAndChooseOne(request1, request2);

  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(kRequestId2, GetNotifier()->last_expired_request().request_id());
  EXPECT_EQ(RequestNotifier::BackgroundSavePageResult::EXPIRED,
            GetNotifier()->last_request_expiration_status());
  EXPECT_EQ(1, GetNotifier()->total_expired_requests());
}

TEST_F(RequestPickerTest, ChooseRequestThatHasNotExceededStartLimit) {
  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(1);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  // With default policy settings, we should choose the earlier request.
  // However, we will make the earlier reqeust exceed the limit.
  request1.set_started_attempt_count(policy_->GetMaxStartedTries());

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}

TEST_F(RequestPickerTest, ChooseRequestThatHasNotExceededCompletionLimit) {
  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(1);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  // With default policy settings, we should choose the earlier request.
  // However, we will make the earlier reqeust exceed the limit.
  request1.set_completed_attempt_count(policy_->GetMaxCompletedTries());

  QueueRequestsAndChooseOne(request1, request2);

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}


TEST_F(RequestPickerTest, ChooseRequestThatIsNotDisabled) {
  policy_.reset(new OfflinerPolicy(kPreferUntried, kPreferEarlier,
                                   kPreferRetryCount, kMaxStartedTries,
                                   kMaxCompletedTries + 1));
  picker_.reset(new RequestPicker(queue_.get(), policy_.get(), notifier_.get(),
                                  &event_logger_));

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(
      kRequestId1, kUrl1, kClientId1, creation_time, kUserRequested);
  SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, creation_time, kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  // put request 2 on disabled list, ensure request1 picked instead,
  // even though policy would prefer 2.
  std::set<int64_t> disabled_requests {kRequestId2};
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
      base::Bind(&RequestPickerTest::RequestNotPicked, base::Unretained(this)),
      &conditions, disabled_requests);

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
}
}  // namespace offline_pages
