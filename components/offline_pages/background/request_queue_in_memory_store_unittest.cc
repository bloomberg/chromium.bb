// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_in_memory_store.h"

#include <memory>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using UpdateStatus = RequestQueueStore::UpdateStatus;

namespace {
const int64_t kRequestId = 42;
const GURL kUrl("http://example.com");
const ClientId kClientId("bookmark", "1234");

bool operator==(const SavePageRequest& lhs, const SavePageRequest& rhs) {
  return lhs.request_id() == rhs.request_id() && lhs.url() == rhs.url() &&
         lhs.client_id() == rhs.client_id() &&
         lhs.creation_time() == rhs.creation_time() &&
         lhs.activation_time() == rhs.activation_time() &&
         lhs.attempt_count() == rhs.attempt_count() &&
         lhs.last_attempt_time() == rhs.last_attempt_time();
}

}  // namespace

class RequestQueueInMemoryStoreTest : public testing::Test {
 public:
  enum class LastResult {
    kNone,
    kFalse,
    kTrue,
  };

  RequestQueueInMemoryStoreTest();

  // Test overrides.
  void SetUp() override;

  RequestQueueStore* store() { return store_.get(); }

  void PumpLoop();
  void ResetResults();

  // Callback used for get requests.
  void GetRequestsDone(bool result,
                       const std::vector<SavePageRequest>& requests);
  // Callback used for add/update request.
  void AddOrUpdateDone(UpdateStatus result);
  // Callback used for remove requests.
  void RemoveDone(bool result, int count);
  // Callback used for reset.
  void ResetDone(bool result);

  LastResult last_result() const { return last_result_; }

  UpdateStatus last_update_status() const { return last_update_status_; }

  int last_remove_count() const { return last_remove_count_; }

  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

 private:
  std::unique_ptr<RequestQueueInMemoryStore> store_;
  LastResult last_result_;
  UpdateStatus last_update_status_;
  int last_remove_count_;
  std::vector<SavePageRequest> last_requests_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueInMemoryStoreTest::RequestQueueInMemoryStoreTest()
    : last_result_(LastResult::kNone),
      last_update_status_(UpdateStatus::kFailed),
      last_remove_count_(0),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

void RequestQueueInMemoryStoreTest::SetUp() {
  store_.reset(new RequestQueueInMemoryStore());
}

void RequestQueueInMemoryStoreTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueInMemoryStoreTest::ResetResults() {
  last_result_ = LastResult::kNone;
  last_update_status_ = UpdateStatus::kFailed;
  last_remove_count_ = 0;
  last_requests_.clear();
}

void RequestQueueInMemoryStoreTest::GetRequestsDone(
    bool result,
    const std::vector<SavePageRequest>& requests) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
  last_requests_ = requests;
}

void RequestQueueInMemoryStoreTest::AddOrUpdateDone(UpdateStatus status) {
  last_update_status_ = status;
}

void RequestQueueInMemoryStoreTest::RemoveDone(bool result, int count) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
  last_remove_count_ = count;
}

void RequestQueueInMemoryStoreTest::ResetDone(bool result) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
}

TEST_F(RequestQueueInMemoryStoreTest, GetRequestsEmpty) {
  store()->GetRequests(base::Bind(
      &RequestQueueInMemoryStoreTest::GetRequestsDone, base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  PumpLoop();
  ASSERT_EQ(LastResult::kTrue, last_result());
  ASSERT_TRUE(last_requests().empty());
}

TEST_F(RequestQueueInMemoryStoreTest, AddRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time);

  store()->AddOrUpdateRequest(
      request, base::Bind(&RequestQueueInMemoryStoreTest::AddOrUpdateDone,
                          base::Unretained(this)));
  ASSERT_EQ(UpdateStatus::kFailed, last_update_status());
  PumpLoop();
  ASSERT_EQ(UpdateStatus::kAdded, last_update_status());

  // Verifying get reqeust results after a request was added.
  ResetResults();
  store()->GetRequests(base::Bind(
      &RequestQueueInMemoryStoreTest::GetRequestsDone, base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  PumpLoop();
  ASSERT_EQ(LastResult::kTrue, last_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_TRUE(request == last_requests()[0]);
}

TEST_F(RequestQueueInMemoryStoreTest, UpdateRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store()->AddOrUpdateRequest(
      original_request,
      base::Bind(&RequestQueueInMemoryStoreTest::AddOrUpdateDone,
                 base::Unretained(this)));
  PumpLoop();
  ResetResults();

  base::Time new_creation_time =
      creation_time + base::TimeDelta::FromMinutes(1);
  base::Time activation_time = creation_time + base::TimeDelta::FromHours(6);
  SavePageRequest updated_request(kRequestId, kUrl, kClientId,
                                  new_creation_time, activation_time);
  store()->AddOrUpdateRequest(
      updated_request,
      base::Bind(&RequestQueueInMemoryStoreTest::AddOrUpdateDone,
                 base::Unretained(this)));
  ASSERT_EQ(UpdateStatus::kFailed, last_update_status());
  PumpLoop();
  ASSERT_EQ(UpdateStatus::kUpdated, last_update_status());

  // Verifying get reqeust results after a request was updated.
  ResetResults();
  store()->GetRequests(base::Bind(
      &RequestQueueInMemoryStoreTest::GetRequestsDone, base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  PumpLoop();
  ASSERT_EQ(LastResult::kTrue, last_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_TRUE(updated_request == last_requests()[0]);
}

TEST_F(RequestQueueInMemoryStoreTest, RemoveRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store()->AddOrUpdateRequest(
      original_request,
      base::Bind(&RequestQueueInMemoryStoreTest::AddOrUpdateDone,
                 base::Unretained(this)));
  PumpLoop();
  ResetResults();

  std::vector<int64_t> request_ids{kRequestId};
  store()->RemoveRequests(request_ids,
                          base::Bind(&RequestQueueInMemoryStoreTest::RemoveDone,
                                     base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  ASSERT_EQ(0, last_remove_count());
  PumpLoop();
  ASSERT_EQ(LastResult::kTrue, last_result());
  ASSERT_EQ(1, last_remove_count());
  ASSERT_EQ(0ul, last_requests().size());
  ResetResults();

  // Removing a request that is missing fails.
  store()->RemoveRequests(request_ids,
                          base::Bind(&RequestQueueInMemoryStoreTest::RemoveDone,
                                     base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  ASSERT_EQ(0, last_remove_count());
  PumpLoop();
  ASSERT_EQ(LastResult::kFalse, last_result());
  ASSERT_EQ(0, last_remove_count());
}

TEST_F(RequestQueueInMemoryStoreTest, ResetStore) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store()->AddOrUpdateRequest(
      original_request,
      base::Bind(&RequestQueueInMemoryStoreTest::AddOrUpdateDone,
                 base::Unretained(this)));
  PumpLoop();
  ResetResults();

  store()->Reset(base::Bind(&RequestQueueInMemoryStoreTest::ResetDone,
                            base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, last_result());
  PumpLoop();
  ASSERT_EQ(LastResult::kTrue, last_result());
  ASSERT_EQ(0ul, last_requests().size());
}

}  // offline_pages
