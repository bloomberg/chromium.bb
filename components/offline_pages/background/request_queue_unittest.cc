// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include <memory>

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
const int64_t kRetryCount = 2;
// Data for request 2.
const int64_t kRequestId2 = 77;
const GURL kUrl2("http://test.com");
const ClientId kClientId2("bookmark", "567");
const bool kUserRequested = true;
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
                       const std::vector<SavePageRequest>& requests);
  // Callback for removing request.
  void RemoveRequestsDone(
      const RequestQueue::UpdateMultipleRequestResults& results,
      const std::vector<SavePageRequest>& requests);

  void UpdateMultipleRequestsDone(
      const RequestQueue::UpdateMultipleRequestResults& results,
      const std::vector<SavePageRequest>& requests);

  void UpdateRequestDone(UpdateRequestResult result);

  RequestQueue* queue() { return queue_.get(); }

  AddRequestResult last_add_result() const { return last_add_result_; }
  SavePageRequest* last_added_request() {
    return last_added_request_.get();
  }

  const RequestQueue::UpdateMultipleRequestResults& last_remove_results()
      const {
    return last_remove_results_;
  }

  const RequestQueue::UpdateMultipleRequestResults&
  last_multiple_update_results() const {
    return last_multiple_update_results_;
  }

  UpdateRequestResult last_update_result() const { return last_update_result_; }

  GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }
  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

 private:
  AddRequestResult last_add_result_;
  std::unique_ptr<SavePageRequest> last_added_request_;
  RequestQueue::UpdateMultipleRequestResults last_remove_results_;
  RequestQueue::UpdateMultipleRequestResults last_multiple_update_results_;
  UpdateRequestResult last_update_result_;

  GetRequestsResult last_get_requests_result_;
  std::vector<SavePageRequest> last_requests_;

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
    const std::vector<SavePageRequest>& requests) {
  last_get_requests_result_ = result;
  last_requests_ = requests;
}

void RequestQueueTest::RemoveRequestsDone(
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  last_remove_results_ = results;
  last_requests_ = requests;
}

void RequestQueueTest::UpdateMultipleRequestsDone(
    const RequestQueue::UpdateMultipleRequestResults& results,
    const std::vector<SavePageRequest>& requests) {
  last_multiple_update_results_ = results;
  last_requests_ = requests;
}

void RequestQueueTest::UpdateRequestDone(UpdateRequestResult result) {
  last_update_result_ = result;
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

  std::vector<int64_t> remove_requests;
  remove_requests.push_back(kRequestId);
  queue()->RemoveRequests(remove_requests,
                          base::Bind(&RequestQueueTest::RemoveRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, last_remove_results().size());
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_results().at(0).second);

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
  queue()->RemoveRequests(remove_requests,
                          base::Bind(&RequestQueueTest::RemoveRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(2ul, last_remove_results().size());
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_results().at(0).second);
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_results().at(1).second);
  ASSERT_EQ(kRequestId, last_remove_results().at(0).first);
  ASSERT_EQ(kRequestId2, last_remove_results().at(1).first);

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
  queue()->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&RequestQueueTest::UpdateMultipleRequestsDone,
                 base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, last_multiple_update_results().size());
  ASSERT_EQ(UpdateRequestResult::SUCCESS,
            last_multiple_update_results().at(0).second);

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Verify the request is paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::PAUSED,
            last_requests().front().request_state());

  // Resume the request.
  queue()->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::AVAILABLE,
      base::Bind(&RequestQueueTest::UpdateMultipleRequestsDone,
                 base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, last_multiple_update_results().size());
  ASSERT_EQ(UpdateRequestResult::SUCCESS,
            last_multiple_update_results().at(0).second);

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Verify the request is no longer paused.
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE,
            last_requests().front().request_state());
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
                          base::Bind(&RequestQueueTest::RemoveRequestsDone,
                                     base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(1ul, last_remove_results().size());
  ASSERT_EQ(kRequestId, last_remove_results().at(0).first);
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_results().at(0).second);

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(request2.request_id(), last_requests()[0].request_id());
}

TEST_F(RequestQueueTest, UpdateRequest) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();

  // Update the request, ensure it succeeded.
  request.set_completed_attempt_count(kRetryCount);
  queue()->UpdateRequest(
      request,
      base::Bind(&RequestQueueTest::UpdateRequestDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_update_result());

  // Get the request, and verify the update took effect.
  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(kRetryCount, last_requests().front().completed_attempt_count());
}

TEST_F(RequestQueueTest, UpdateRequestNotPresent) {
  // First add a request.  Retry count will be set to 0.
  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  SavePageRequest request2(
      kRequestId2, kUrl2, kClientId2, creation_time, kUserRequested);
  queue()->AddRequest(request2, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();

  // Try to update request1 when only request2 is in the queue.
  queue()->UpdateRequest(
      request1,
      base::Bind(&RequestQueueTest::UpdateRequestDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(UpdateRequestResult::REQUEST_DOES_NOT_EXIST, last_update_result());
}

}  // namespace offline_pages
