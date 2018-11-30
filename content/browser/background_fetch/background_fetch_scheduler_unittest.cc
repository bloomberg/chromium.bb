// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_scheduler.h"

#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

using ::testing::ElementsAre;

namespace content {

class FakeController : public BackgroundFetchJobController {
 public:
  FakeController(BackgroundFetchDataManager* data_manager,
                 BackgroundFetchDelegateProxy* delegate_proxy,
                 const BackgroundFetchRegistrationId& registration_id,
                 std::vector<std::string>* controller_sequence_list,
                 FinishedCallback finished_callback)
      : BackgroundFetchJobController(data_manager,
                                     delegate_proxy,
                                     registration_id,
                                     BackgroundFetchOptions(),
                                     SkBitmap(),
                                     0ul,
                                     base::DoNothing(),
                                     std::move(finished_callback)),
        controller_sequence_list_(controller_sequence_list) {
    DCHECK(controller_sequence_list_);
  }

  ~FakeController() override = default;

  void DidCompleteRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request) override {
    // Record the completed request. Store everything after the origin and the
    // slash, to be able to directly compare with the provided requests.
    controller_sequence_list_->push_back(
        request->fetch_request().url.path().substr(1));

    // Continue normally.
    BackgroundFetchJobController::DidCompleteRequest(request);
  }

 private:
  std::vector<std::string>* controller_sequence_list_;
};

class BackgroundFetchSchedulerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchSchedulerTest() = default;

  void SetUp() override {
    BackgroundFetchTestBase::SetUp();
    data_manager_ = std::make_unique<BackgroundFetchTestDataManager>(
        browser_context(), storage_partition(),
        embedded_worker_test_helper()->context_wrapper());
    data_manager_->InitializeOnIOThread();

    delegate_proxy_ = std::make_unique<BackgroundFetchDelegateProxy>(
        browser_context()->GetBackgroundFetchDelegate());

    scheduler_ = std::make_unique<BackgroundFetchScheduler>(
        data_manager_.get(), nullptr, delegate_proxy_.get(),
        embedded_worker_test_helper()->context_wrapper());
  }

  void TearDown() override {
    data_manager_ = nullptr;
    delegate_proxy_ = nullptr;
    scheduler_ = nullptr;
    controller_sequence_list_.clear();
    BackgroundFetchTestBase::TearDown();
  }

 protected:
  void InitializeControllerWithRequests(
      const std::vector<std::string>& requests) {
    std::vector<blink::mojom::FetchAPIRequestPtr> fetch_requests;
    for (auto& request : requests) {
      auto fetch_request = blink::mojom::FetchAPIRequest::New();
      fetch_request->referrer = blink::mojom::Referrer::New();
      fetch_request->url = GURL(origin().GetURL().spec() + request);
      CreateRequestWithProvidedResponse(fetch_request->method,
                                        fetch_request->url,
                                        TestResponseBuilder(200).Build());
      fetch_requests.push_back(std::move(fetch_request));
    }

    int64_t sw_id = RegisterServiceWorker();
    BackgroundFetchRegistrationId registration_id(
        sw_id, origin(), base::GenerateGUID(), base::GenerateGUID());
    data_manager_->CreateRegistration(
        registration_id, std::move(fetch_requests), BackgroundFetchOptions(),
        SkBitmap(),
        /* start_paused= */ false, base::DoNothing());
    thread_bundle_.RunUntilIdle();

    auto controller = std::make_unique<FakeController>(
        data_manager_.get(), delegate_proxy_.get(), registration_id,
        &controller_sequence_list_,
        base::BindOnce(&BackgroundFetchSchedulerTest::DidJobFinish,
                       base::Unretained(this)));
    controller->InitializeRequestStatus(0 /* completed_downloads */,
                                        requests.size(),
                                        {} /* active_fetch_requests */,
                                        /* start_paused= */ false);
    scheduler_->job_controllers_[registration_id.unique_id()] =
        std::move(controller);
    scheduler_->controller_ids_.push_back(registration_id.unique_id());
  }

  void RunSchedulerToCompletion() {
    scheduler_->ScheduleDownload();
    thread_bundle_.RunUntilIdle();
  }

  void DidJobFinish(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchFailureReason failure_reason,
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback) {
    DCHECK_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);
    scheduler_->job_controllers_.erase(registration_id.unique_id());
    scheduler_->active_controller_ = nullptr;
    scheduler_->ScheduleDownload();
  }

 protected:
  std::vector<std::string> controller_sequence_list_;

 private:
  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  std::unique_ptr<BackgroundFetchTestDataManager> data_manager_;
  std::unique_ptr<BackgroundFetchScheduler> scheduler_;
};

TEST_F(BackgroundFetchSchedulerTest, SingleController) {
  std::vector<std::string> requests = {"A1", "A2", "A3", "A4"};
  InitializeControllerWithRequests(requests);
  RunSchedulerToCompletion();
  EXPECT_EQ(requests, controller_sequence_list_);
}

TEST_F(BackgroundFetchSchedulerTest, TwoControllers) {
  std::vector<std::string> all_requests = {"A1", "A2", "A3", "A4",
                                           "B1", "B2", "B3"};

  // Create a controller with A1 -> A4.
  InitializeControllerWithRequests(
      std::vector<std::string>(all_requests.begin(), all_requests.begin() + 4));

  // Create a controller with B1 -> B4.
  InitializeControllerWithRequests(
      std::vector<std::string>(all_requests.begin() + 4, all_requests.end()));

  RunSchedulerToCompletion();
  EXPECT_EQ(all_requests, controller_sequence_list_);
}

}  // namespace content
