// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using AddRequestResult = RequestQueue::AddRequestResult;
using GetRequestsResult = RequestQueue::GetRequestsResult;
using UpdateRequestResult = RequestQueue::UpdateRequestResult;

namespace {
// Data for request 1.
const int64_t kRequestId = 42;
const GURL kUrl("http://example.com");
const ClientId kClientId("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 77;
const GURL kUrl2("http://test.com");
const ClientId kClientId2("bookmark", "567");
const bool kUserRequested = true;
const int64_t kRequestId3 = 99;
}  // namespace

// TODO(fgorski): Add tests for store failures in add/remove/get.
class RequestQueueTest : public testing::Test {
 public:
  RequestQueueTest();
  ~RequestQueueTest() override;

  // Test overrides.
  void SetUp() override;

  void PumpLoop();

  // Callback for adding requests.
  void AddRequestDone(AddRequestResult result, const SavePageRequest& request);
  // Callback for getting requests.
  void GetRequestsDone(GetRequestsResult result,
                       std::vector<std::unique_ptr<SavePageRequest>> requests);

  void UpdateRequestDone(UpdateRequestResult result);
  void UpdateRequestsDone(std::unique_ptr<UpdateRequestsResult> result);

  void ClearResults();

  RequestQueue* queue() { return queue_.get(); }

  AddRequestResult last_add_result() const { return last_add_result_; }
  SavePageRequest* last_added_request() {
    return last_added_request_.get();
  }

  UpdateRequestResult last_update_result() const { return last_update_result_; }

  GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return last_requests_;
  }

  UpdateRequestsResult* update_requests_result() const {
    return update_requests_result_.get();
  }

 private:
  AddRequestResult last_add_result_;
  std::unique_ptr<SavePageRequest> last_added_request_;
  std::unique_ptr<UpdateRequestsResult> update_requests_result_;
  UpdateRequestResult last_update_result_;

  GetRequestsResult last_get_requests_result_;
  std::vector<std::unique_ptr<SavePageRequest>> last_requests_;

  std::unique_ptr<RequestQueue> queue_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueTest::RequestQueueTest()
    : last_add_result_(AddRequestResult::STORE_FAILURE),
      last_update_result_(UpdateRequestResult::STORE_FAILURE),
      last_get_requests_result_(GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestQueueTest::~RequestQueueTest() {}

void RequestQueueTest::SetUp() {
  std::unique_ptr<RequestQueueInMemoryStore> store(
      new RequestQueueInMemoryStore());
  queue_.reset(new RequestQueue(std::move(store)));
}

void RequestQueueTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueTest::AddRequestDone(AddRequestResult result,
                                      const SavePageRequest& request) {
  last_add_result_ = result;
  last_added_request_.reset(new SavePageRequest(request));
}

void RequestQueueTest::GetRequestsDone(
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_get_requests_result_ = result;
  last_requests_ = std::move(requests);
}

void RequestQueueTest::UpdateRequestDone(UpdateRequestResult result) {
  last_update_result_ = result;
}

void RequestQueueTest::UpdateRequestsDone(
    std::unique_ptr<UpdateRequestsResult> result) {
  update_requests_result_ = std::move(result);
}

void RequestQueueTest::ClearResults() {
  last_add_result_ = AddRequestResult::STORE_FAILURE;
  last_update_result_ = UpdateRequestResult::STORE_FAILURE;
  last_get_requests_result_ = GetRequestsResult::STORE_FAILURE;
  last_added_request_.reset(nullptr);
  update_requests_result_.reset(nullptr);
  last_requests_.clear();
}

TEST_F(RequestQueueTest, GetRequestsEmpty) {
  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, AddRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(AddRequestResult::SUCCESS, last_add_result());
  ASSERT_TRUE(last_added_request());
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
}

TEST_F(RequestQueueTest, RemoveRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  std::vector<int64_t> remove_requests{kRequestId};
  queue()->RemoveRequests(remove_requests,
                          base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(request, update_requests_result()->updated_items.at(0));

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, RemoveSeveralRequests) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time,
                           kUserRequested);
  queue()->AddRequest(request2, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId2, last_added_request()->request_id());

  std::vector<int64_t> remove_requests;
  remove_requests.push_back(kRequestId);
  remove_requests.push_back(kRequestId2);
  remove_requests.push_back(kRequestId3);
  queue()->RemoveRequests(remove_requests,
                          base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(3ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(kRequestId2, update_requests_result()->item_statuses.at(1).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(1).second);
  ASSERT_EQ(kRequestId3, update_requests_result()->item_statuses.at(2).first);
  ASSERT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(2).second);
  EXPECT_EQ(2UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(request, update_requests_result()->updated_items.at(0));
  EXPECT_EQ(request2, update_requests_result()->updated_items.at(1));

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Verify both requests are no longer in the queue.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, PauseAndResume) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());

  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId);

  // Pause the request.
  queue()->ChangeRequestsState(request_ids,
                               SavePageRequest::RequestState::PAUSED,
                               base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(1ul, update_requests_result()->updated_items.size());
  ASSERT_EQ(SavePageRequest::RequestState::PAUSED,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Verify the request is paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::PAUSED,
            last_requests().at(0)->request_state());

  // Resume the request.
  queue()->ChangeRequestsState(request_ids,
                               SavePageRequest::RequestState::AVAILABLE,
                               base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  ASSERT_EQ(1ul, update_requests_result()->updated_items.size());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Verify the request is no longer paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE,
            last_requests().at(0)->request_state());
}

// A longer test populating the request queue with more than one item, properly
// listing multiple items and removing the right item.
TEST_F(RequestQueueTest, MultipleRequestsAddGetRemove) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  queue()->AddRequest(request1, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request1.request_id(), last_added_request()->request_id());
  SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, creation_time, kUserRequested);
  queue()->AddRequest(request2, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request2.request_id(), last_added_request()->request_id());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(2ul, last_requests().size());

  std::vector<int64_t> remove_requests;
  remove_requests.push_back(request1.request_id());
  queue()->RemoveRequests(remove_requests,
                          base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  ASSERT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  ASSERT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(request2.request_id(), last_requests().at(0)->request_id());
}

TEST_F(RequestQueueTest, MarkAttemptStarted) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();

  base::Time before_time = base::Time::Now();
  // Update the request, ensure it succeeded.
  queue()->MarkAttemptStarted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_LE(before_time,
            update_requests_result()->updated_items.at(0).last_attempt_time());
  EXPECT_GE(base::Time::Now(),
            update_requests_result()->updated_items.at(0).last_attempt_time());
  EXPECT_EQ(
      1, update_requests_result()->updated_items.at(0).started_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::PRERENDERING,
            update_requests_result()->updated_items.at(0).request_state());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  EXPECT_EQ(update_requests_result()->updated_items.at(0),
            *last_requests().at(0));
}

TEST_F(RequestQueueTest, MarkAttempStartedRequestNotPresent) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = base::Time::Now();
  // This request is never put into the queue.
  SavePageRequest request1(kRequestId, kUrl, kClientId, creation_time,
                           kUserRequested);

  queue()->MarkAttemptStarted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(0ul, update_requests_result()->updated_items.size());
}

TEST_F(RequestQueueTest, MarkAttemptAborted) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();

  // Start request.
  queue()->MarkAttemptStarted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ClearResults();

  queue()->MarkAttemptAborted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();

  ASSERT_TRUE(update_requests_result());
  EXPECT_EQ(1UL, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());
}

TEST_F(RequestQueueTest, MarkAttemptAbortedRequestNotPresent) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = base::Time::Now();
  // This request is never put into the queue.
  SavePageRequest request1(kRequestId, kUrl, kClientId, creation_time,
                           kUserRequested);

  queue()->MarkAttemptAborted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(0ul, update_requests_result()->updated_items.size());
}

TEST_F(RequestQueueTest, MarkAttemptCompleted) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();

  // Start request.
  queue()->MarkAttemptStarted(kRequestId,
                              base::Bind(&RequestQueueTest::UpdateRequestsDone,
                                         base::Unretained(this)));
  PumpLoop();
  ClearResults();

  queue()->MarkAttemptCompleted(
      kRequestId, base::Bind(&RequestQueueTest::UpdateRequestsDone,
                             base::Unretained(this)));
  PumpLoop();

  ASSERT_TRUE(update_requests_result());
  EXPECT_EQ(1UL, update_requests_result()->item_statuses.size());
  EXPECT_EQ(kRequestId, update_requests_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            update_requests_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, update_requests_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE,
            update_requests_result()->updated_items.at(0).request_state());
}

}  // namespace offline_pages
