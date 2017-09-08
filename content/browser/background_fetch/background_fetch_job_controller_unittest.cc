// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_delegate_impl.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace content {
namespace {

const char kExampleTag[] = "my-example-tag";

class BackgroundFetchJobControllerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchJobControllerTest()
      : data_manager_(browser_context(),
                      embedded_worker_test_helper()->context_wrapper()) {}
  ~BackgroundFetchJobControllerTest() override = default;

  // Creates a new Background Fetch registration, whose id will be stored in
  // the |*registration_id|, and registers it with the DataManager for the
  // included |request_data|. Should be wrapped in ASSERT_NO_FATAL_FAILURE().
  void CreateRegistrationForRequests(
      BackgroundFetchRegistrationId* registration_id,
      std::map<std::string /* url */, std::string /* method */> request_data) {
    DCHECK(registration_id);

    ASSERT_TRUE(CreateRegistrationId(kExampleTag, registration_id));

    std::vector<ServiceWorkerFetchRequest> requests;
    requests.reserve(request_data.size());

    for (const auto& pair : request_data) {
      requests.emplace_back(GURL(pair.first), pair.second /* method */,
                            ServiceWorkerHeaderMap(), Referrer(),
                            false /* is_reload */);
    }

    blink::mojom::BackgroundFetchError error;

    base::RunLoop run_loop;
    data_manager_.CreateRegistration(
        *registration_id, requests, BackgroundFetchOptions(),
        base::BindOnce(&BackgroundFetchJobControllerTest::DidCreateRegistration,
                       base::Unretained(this), &error, run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Provide fake responses for the given |request_data| pairs.
    for (const auto& pair : request_data) {
      CreateRequestWithProvidedResponse(
          pair.second, pair.first,
          TestResponseBuilder(200 /* response_code */).Build());
    }
  }

  // Creates a new BackgroundFetchJobController instance.
  std::unique_ptr<BackgroundFetchJobController> CreateJobController(
      const BackgroundFetchRegistrationId& registration_id) {
    delegate_ =
        base::MakeUnique<BackgroundFetchDelegateImpl>(browser_context());
    delegate_proxy_ =
        base::MakeUnique<BackgroundFetchDelegateProxy>(delegate_->GetWeakPtr());

    return base::MakeUnique<BackgroundFetchJobController>(
        delegate_proxy_.get(), registration_id, BackgroundFetchOptions(),
        &data_manager_,
        base::BindOnce(&BackgroundFetchJobControllerTest::DidCompleteJob,
                       base::Unretained(this)));
  }

 protected:
  BackgroundFetchDataManager data_manager_;
  bool did_complete_job_ = false;

  // Closure that will be invoked when the JobController has completed all
  // available jobs. Enables use of a run loop for deterministic waits.
  base::OnceClosure job_completed_closure_;

  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  std::unique_ptr<BackgroundFetchDelegateImpl> delegate_;

 private:
  void DidCreateRegistration(blink::mojom::BackgroundFetchError* out_error,
                             const base::Closure& quit_closure,
                             blink::mojom::BackgroundFetchError error) {
    DCHECK(out_error);

    *out_error = error;

    quit_closure.Run();
  }

  void DidCompleteJob(BackgroundFetchJobController* controller) {
    DCHECK(controller);
    EXPECT_TRUE(
        controller->state() == BackgroundFetchJobController::State::ABORTED ||
        controller->state() == BackgroundFetchJobController::State::COMPLETED);

    did_complete_job_ = true;

    if (job_completed_closure_)
      std::move(job_completed_closure_).Run();
  }

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobControllerTest);
};

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{"https://example.com/funny_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);

  controller->Start();
  EXPECT_EQ(controller->state(), BackgroundFetchJobController::State::FETCHING);

  // Mark the single download item as finished, completing the job.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    run_loop.Run();
  }

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::COMPLETED);
  EXPECT_TRUE(did_complete_job_);
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  // This test should always issue more requests than the number of allowed
  // parallel requests. That way we ensure testing the iteration behaviour.
  ASSERT_GT(5u, kMaximumBackgroundFetchParallelRequests);

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{"https://example.com/funny_cat.png", "GET"},
                         {"https://example.com/scary_cat.png", "GET"},
                         {"https://example.com/crazy_cat.png", "GET"},
                         {"https://example.com/silly_cat.png", "GET"},
                         {"https://example.com/happy_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);

  // Continue spinning until the Job Controller has completed all the requests.
  // The Download Manager has been told to automatically mark them as finished.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    controller->Start();
    EXPECT_EQ(controller->state(),
              BackgroundFetchJobController::State::FETCHING);

    run_loop.Run();
  }

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::COMPLETED);
  EXPECT_TRUE(did_complete_job_);
}

TEST_F(BackgroundFetchJobControllerTest, AbortJob) {
  BackgroundFetchRegistrationId registration_id;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{"https://example.com/sad_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);

  // Start the first few requests, and abort them immediately after.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    controller->Start();
    EXPECT_EQ(controller->state(),
              BackgroundFetchJobController::State::FETCHING);

    controller->Abort();

    run_loop.Run();
  }

  // TODO(peter): Verify that the issued download items have had their state
  // updated to be cancelled as well.

  EXPECT_EQ(controller->state(), BackgroundFetchJobController::State::ABORTED);
  EXPECT_TRUE(did_complete_job_);
}

}  // namespace
}  // namespace content
