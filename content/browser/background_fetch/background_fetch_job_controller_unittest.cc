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
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace content {
namespace {

const char kExampleDeveloperId[] = "my-example-id";
const char kExampleResponseData[] = "My response data";

class BackgroundFetchJobControllerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchJobControllerTest()
      : data_manager_(browser_context(),
                      embedded_worker_test_helper()->context_wrapper()) {}
  ~BackgroundFetchJobControllerTest() override = default;

  // Creates a new Background Fetch registration, whose id will be stored in the
  // |*registration_id|, and registers it with the DataManager for the included
  // |request_data|. If |auto_complete_requests| is true, the request will
  // immediately receive a successful response. Should be wrapped in
  // ASSERT_NO_FATAL_FAILURE().
  void CreateRegistrationForRequests(
      BackgroundFetchRegistrationId* registration_id,
      std::map<GURL, std::string /* method */> request_data,
      bool auto_complete_requests) {
    DCHECK(registration_id);

    int64_t service_worker_registration_id = RegisterServiceWorker();
    ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
              service_worker_registration_id);

    // New |unique_id|, since this is a new Background Fetch registration.
    *registration_id = BackgroundFetchRegistrationId(
        service_worker_registration_id, origin(), kExampleDeveloperId,
        base::GenerateGUID());

    std::vector<ServiceWorkerFetchRequest> requests;
    requests.reserve(request_data.size());

    for (const auto& pair : request_data) {
      requests.emplace_back(pair.first, pair.second /* method */,
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

    if (auto_complete_requests) {
      // Provide fake responses for the given |request_data| pairs.
      for (const auto& pair : request_data) {
        CreateRequestWithProvidedResponse(
            pair.second /* method */, pair.first /* url */,
            TestResponseBuilder(200 /* response_code */)
                .SetResponseData(kExampleResponseData)
                .Build());
      }
    }
  }

  // Creates a new BackgroundFetchJobController instance.
  std::unique_ptr<BackgroundFetchJobController> CreateJobController(
      const BackgroundFetchRegistrationId& registration_id,
      int total_downloads = 1) {
    delegate_ = browser_context()->GetBackgroundFetchDelegate();
    DCHECK(delegate_);
    delegate_proxy_ = std::make_unique<BackgroundFetchDelegateProxy>(delegate_);

    BackgroundFetchRegistration registration;
    registration.developer_id = registration_id.developer_id();
    registration.unique_id = registration_id.unique_id();

    auto controller = std::make_unique<BackgroundFetchJobController>(
        delegate_proxy_.get(), registration_id, BackgroundFetchOptions(),
        registration, &data_manager_,
        base::BindRepeating(
            &BackgroundFetchJobControllerTest::DidUpdateProgress,
            base::Unretained(this)),
        base::BindOnce(&BackgroundFetchJobControllerTest::DidFinishJob,
                       base::Unretained(this)));

    controller->InitializeRequestStatus(0, total_downloads,
                                        std::vector<std::string>());
    return controller;
  }

 protected:
  BackgroundFetchDataManager data_manager_;

  uint64_t last_downloaded_ = 0;
  bool did_finish_job_ = false;
  bool was_aborted_ = false;

  // Closure that will be invoked every time the JobController receives a
  // progress update from a download.
  base::RepeatingClosure job_progress_closure_;

  // Closure that will be invoked when the JobController has completed all
  // available jobs. Enables use of a run loop for deterministic waits.
  base::OnceClosure job_completed_closure_;

  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;
  BackgroundFetchDelegate* delegate_;

 private:
  void DidCreateRegistration(
      blink::mojom::BackgroundFetchError* out_error,
      const base::Closure& quit_closure,
      blink::mojom::BackgroundFetchError error,
      std::unique_ptr<BackgroundFetchRegistration> registration) {
    DCHECK(out_error);

    *out_error = error;

    quit_closure.Run();
  }

  void DidUpdateProgress(const std::string& unique_id,
                         uint64_t download_total,
                         uint64_t downloaded) {
    last_downloaded_ = downloaded;

    if (job_progress_closure_)
      job_progress_closure_.Run();
  }

  void DidFinishJob(const BackgroundFetchRegistrationId& registration_id,
                    bool aborted) {
    did_finish_job_ = true;
    was_aborted_ = aborted;

    if (job_completed_closure_)
      std::move(job_completed_closure_).Run();
  }

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobControllerTest);
};

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/funny_cat.png"), "GET"}},
      true /* auto_complete_requests */));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  controller->Start();

  // Continue spinning until the Job Controller has completed the request.
  // The Delegate has been told to automatically mark it as finished.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    run_loop.Run();
  }

  EXPECT_TRUE(did_finish_job_);
  EXPECT_FALSE(was_aborted_);
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchRegistrationId registration_id;

  // This test should always issue more requests than the number of allowed
  // parallel requests. That way we ensure testing the iteration behaviour.
  ASSERT_GT(5u, kMaximumBackgroundFetchParallelRequests);

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id,
      {{GURL("https://example.com/funny_cat.png"), "GET"},
       {GURL("https://example.com/scary_cat.png"), "GET"},
       {GURL("https://example.com/crazy_cat.png"), "GET"},
       {GURL("https://example.com/silly_cat.png"), "GET"},
       {GURL("https://example.com/happy_cat.png"), "GET"}},
      true /* auto_complete_requests */));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  // Continue spinning until the Job Controller has completed all the requests.
  // The Delegate has been told to automatically mark them as finished.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    controller->Start();

    run_loop.Run();
  }

  EXPECT_TRUE(did_finish_job_);
  EXPECT_FALSE(was_aborted_);
}

TEST_F(BackgroundFetchJobControllerTest, Abort) {
  BackgroundFetchRegistrationId registration_id;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/sad_cat.png"), "GET"}},
      false /* auto_complete_requests */));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  // Start the request, and abort it immediately after.
  controller->Start();

  // TODO(delphick): Add test for AbortFromUser as well.
  controller->Abort();

  // Run until idle to ensure that spurious download successful tasks are not
  // executed.
  base::RunLoop().RunUntilIdle();

  // TODO(peter): Verify that the issued download items have had their state
  // updated to be cancelled as well.

  EXPECT_TRUE(did_finish_job_);
  EXPECT_TRUE(was_aborted_);
}

TEST_F(BackgroundFetchJobControllerTest, Progress) {
  BackgroundFetchRegistrationId registration_id;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, {{GURL("https://example.com/slow_cat.png"), "GET"}},
      true /* auto_complete_requests */));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  controller->Start();

  {
    base::RunLoop run_loop;
    job_progress_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  EXPECT_GT(last_downloaded_, 0u);
  EXPECT_LT(last_downloaded_, strlen(kExampleResponseData));

  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  EXPECT_TRUE(did_finish_job_);
  EXPECT_EQ(last_downloaded_, strlen(kExampleResponseData));
}

}  // namespace
}  // namespace content
