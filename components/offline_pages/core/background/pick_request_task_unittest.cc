// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/pick_request_task.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_in_memory_store.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
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
const int kBackgroundProcessingTimeBudgetSeconds = 170;

// Default request
const SavePageRequest kEmptyRequest(0UL,
                                    GURL(""),
                                    ClientId("", ""),
                                    base::Time(),
                                    true);
}  // namespace

// Helper class needed by the PickRequestTask
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

  void NotifyNetworkProgress(const SavePageRequest& request,
                             int64_t received_bytes) override {}

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

class PickRequestTaskTest : public testing::Test {
 public:
  PickRequestTaskTest();

  ~PickRequestTaskTest() override;

  void SetUp() override;

  void PumpLoop();

  void AddRequestDone(ItemActionStatus status);

  void RequestPicked(
      const SavePageRequest& request,
      const std::unique_ptr<std::vector<SavePageRequest>> available_requests,
      bool cleanup_needed);

  void RequestNotPicked(const bool non_user_requested_tasks_remaining,
                        bool cleanup_needed);

  void RequestCountCallback(size_t total_count, size_t available_count);

  void QueueRequests(const SavePageRequest& request1,
                     const SavePageRequest& request2);

  // Reset the factory and the task using the current policy.
  void MakePickRequestTask();

  RequestNotifierStub* GetNotifier() { return notifier_.get(); }

  PickRequestTask* task() { return task_.get(); }

  void TaskCompletionCallback(Task* completed_task);

 protected:
  void InitializeStoreDone(bool success);

  std::unique_ptr<RequestQueueStore> store_;
  std::unique_ptr<RequestNotifierStub> notifier_;
  std::unique_ptr<SavePageRequest> last_picked_;
  std::unique_ptr<OfflinerPolicy> policy_;
  RequestCoordinatorEventLogger event_logger_;
  std::set<int64_t> disabled_requests_;
  base::circular_deque<int64_t> prioritized_requests_;
  std::unique_ptr<PickRequestTask> task_;
  bool request_queue_not_picked_called_;
  bool cleanup_needed_;
  size_t total_request_count_;
  size_t available_request_count_;
  bool task_complete_called_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

PickRequestTaskTest::PickRequestTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

PickRequestTaskTest::~PickRequestTaskTest() {}

void PickRequestTaskTest::SetUp() {
  DeviceConditions conditions;
  store_.reset(new RequestQueueInMemoryStore());
  policy_.reset(new OfflinerPolicy());
  notifier_.reset(new RequestNotifierStub());
  MakePickRequestTask();
  request_queue_not_picked_called_ = false;
  total_request_count_ = 9999;
  available_request_count_ = 9999;
  task_complete_called_ = false;
  last_picked_.reset();
  cleanup_needed_ = false;

  store_->Initialize(base::BindOnce(&PickRequestTaskTest::InitializeStoreDone,
                                    base::Unretained(this)));
  PumpLoop();
}

void PickRequestTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void PickRequestTaskTest::TaskCompletionCallback(Task* completed_task) {
  task_complete_called_ = true;
}

void PickRequestTaskTest::AddRequestDone(ItemActionStatus status) {}

void PickRequestTaskTest::RequestPicked(
    const SavePageRequest& request,
    std::unique_ptr<std::vector<SavePageRequest>> available_requests,
    const bool cleanup_needed) {
  last_picked_.reset(new SavePageRequest(request));
  cleanup_needed_ = cleanup_needed;
}

void PickRequestTaskTest::RequestNotPicked(
    const bool non_user_requested_tasks_remaining,
    const bool cleanup_needed) {
  request_queue_not_picked_called_ = true;
}

void PickRequestTaskTest::RequestCountCallback(size_t total_count,
                                               size_t available_count) {
  total_request_count_ = total_count;
  available_request_count_ = available_count;
}

// Test helper to queue the two given requests.
void PickRequestTaskTest::QueueRequests(const SavePageRequest& request1,
                                        const SavePageRequest& request2) {
  DeviceConditions conditions;
  std::set<int64_t> disabled_requests;
  // Add test requests on the Queue.
  store_->AddRequest(request1,
                     base::BindOnce(&PickRequestTaskTest::AddRequestDone,
                                    base::Unretained(this)));
  store_->AddRequest(request2,
                     base::BindOnce(&PickRequestTaskTest::AddRequestDone,
                                    base::Unretained(this)));

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();
}

void PickRequestTaskTest::MakePickRequestTask() {
  DeviceConditions conditions;
  task_.reset(new PickRequestTask(
      store_.get(), policy_.get(),
      base::BindOnce(&PickRequestTaskTest::RequestPicked,
                     base::Unretained(this)),
      base::BindOnce(&PickRequestTaskTest::RequestNotPicked,
                     base::Unretained(this)),
      base::BindOnce(&PickRequestTaskTest::RequestCountCallback,
                     base::Unretained(this)),
      conditions, disabled_requests_, prioritized_requests_));
  task_->SetTaskCompletionCallbackForTesting(
      task_runner_.get(),
      base::BindRepeating(&PickRequestTaskTest::TaskCompletionCallback,
                          base::Unretained(this)));
}

void PickRequestTaskTest::InitializeStoreDone(bool success) {
  ASSERT_TRUE(success);
}

TEST_F(PickRequestTaskTest, PickFromEmptyQueue) {
  task()->Run();
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "QueueEmpty"
  // callback.
  PumpLoop();

  EXPECT_TRUE(request_queue_not_picked_called_);
  EXPECT_EQ(0UL, total_request_count_);
  EXPECT_EQ(0UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithHigherRetryCount) {
  // Set up policy to prefer higher retry count.
  policy_.reset(new OfflinerPolicy(
      kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds));
  MakePickRequestTask();

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(2UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithSameRetryCountButEarlier) {
  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseEarlierRequest) {
  // We need a custom policy object prefering recency to retry count.
  policy_.reset(new OfflinerPolicy(
      kPreferUntried, kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries, kBackgroundProcessingTimeBudgetSeconds));
  MakePickRequestTask();

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseSameTimeRequestWithHigherRetryCount) {
  // We need a custom policy object preferring recency to retry count.
  policy_.reset(new OfflinerPolicy(
      kPreferUntried, kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds));
  MakePickRequestTask();

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseRequestWithLowerRetryCount) {
  // We need a custom policy object preferring lower retry count.
  policy_.reset(new OfflinerPolicy(
      !kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds));
  MakePickRequestTask();

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseLaterRequest) {
  // We need a custom policy preferring recency over retry, and later requests.
  policy_.reset(new OfflinerPolicy(
      kPreferUntried, !kPreferEarlier, !kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries, kBackgroundProcessingTimeBudgetSeconds));
  MakePickRequestTask();

  base::Time creation_time1 =
      base::Time::Now() - base::TimeDelta::FromSeconds(10);
  base::Time creation_time2 = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time1,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time2,
                           kUserRequested);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChooseNonExpiredRequest) {
  base::Time creation_time = base::Time::Now();
  base::Time expired_time =
      creation_time - base::TimeDelta::FromSeconds(
                          policy_->GetRequestExpirationTimeInSeconds() + 60);
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, expired_time,
                           kUserRequested);

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(1UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatHasNotExceededStartLimit) {
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

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(1UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatHasNotExceededCompletionLimit) {
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

  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_TRUE(cleanup_needed_);
}

TEST_F(PickRequestTaskTest, ChooseRequestThatIsNotDisabled) {
  policy_.reset(new OfflinerPolicy(
      kPreferUntried, kPreferEarlier, kPreferRetryCount, kMaxStartedTries,
      kMaxCompletedTries + 1, kBackgroundProcessingTimeBudgetSeconds));

  // put request 2 on disabled list, ensure request1 picked instead,
  // even though policy would prefer 2.
  disabled_requests_.insert(kRequestId2);
  MakePickRequestTask();

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(1UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
}

TEST_F(PickRequestTaskTest, ChoosePrioritizedRequests) {
  prioritized_requests_.push_back(kRequestId2);
  MakePickRequestTask();

  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  // Since default policy prefer untried requests, make request1 the favorable
  // pick if no prioritized requests. But request2 is prioritized so it should
  // be picked.
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(2UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_EQ(1UL, prioritized_requests_.size());
}

TEST_F(PickRequestTaskTest, ChooseFromTwoPrioritizedRequests) {
  // Make two prioritized requests, the second one should be picked because
  // higher priority requests are later on the list.
  prioritized_requests_.push_back(kRequestId1);
  prioritized_requests_.push_back(kRequestId2);
  MakePickRequestTask();

  // Making request 1 more attractive to be picked not considering the
  // prioritizing issues with older creation time, fewer attempt count and it's
  // earlier in the request queue.
  base::Time creation_time = base::Time::Now();
  base::Time older_creation_time =
      creation_time - base::TimeDelta::FromMinutes(10);
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, older_creation_time,
                           kUserRequested);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);

  // Add test requests on the Queue.
  QueueRequests(request1, request2);

  task()->Run();
  PumpLoop();

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId2, last_picked_->request_id());
  EXPECT_FALSE(request_queue_not_picked_called_);
  EXPECT_EQ(2UL, total_request_count_);
  EXPECT_EQ(2UL, available_request_count_);
  EXPECT_TRUE(task_complete_called_);
  EXPECT_EQ(2UL, prioritized_requests_.size());
}
}  // namespace offline_pages
