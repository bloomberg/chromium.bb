// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_job_response_data.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"

namespace content {
namespace {

const char kResource[] = "https://example.com/resource.html";
const char kTag[] = "TestRequestTag";
const char kJobOrigin[] = "https://example.com";
const int64_t kServiceWorkerRegistrationId = 9001;

}  // namespace

class BackgroundFetchDataManagerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchDataManagerTest()
      : background_fetch_data_manager_(
            base::MakeUnique<BackgroundFetchDataManager>(browser_context())) {}
  ~BackgroundFetchDataManagerTest() override = default;

 protected:
  // Synchronous version of BackgroundFetchDataManager::CreateRegistration().
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      blink::mojom::BackgroundFetchError* out_error,
      std::vector<BackgroundFetchRequestInfo>* out_initial_requests) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidCreateRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_initial_requests));
    run_loop.Run();
  }

  // Synchronous version of BackgroundFetchDataManager::DeleteRegistration().
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->DeleteRegistration(
        registration_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidDeleteRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();
  }

  void CreateRequests(int num_requests) {
    DCHECK_GT(num_requests, 0);
    // Create |num_requests| BackgroundFetchRequestInfo's.
    std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos;
    for (int i = 0; i < num_requests; i++) {
      ServiceWorkerHeaderMap headers;
      ServiceWorkerFetchRequest request(GURL(kResource), "GET", headers,
                                        Referrer(), false /* is_reload */);
      request_infos.push_back(
          base::MakeUnique<BackgroundFetchRequestInfo>(i, request));
    }
    std::unique_ptr<BackgroundFetchJobInfo> job_info =
        base::MakeUnique<BackgroundFetchJobInfo>(
            kTag, url::Origin(GURL(kJobOrigin)), kServiceWorkerRegistrationId);
    job_info->set_num_requests(num_requests);

    job_guid_ = job_info->guid();

    background_fetch_data_manager_->WriteJobToStorage(std::move(job_info),
                                                      std::move(request_infos));
  }

  void GetResponse() {
    base::RunLoop run_loop;
    BackgroundFetchResponseCompleteCallback callback =
        base::Bind(&BackgroundFetchDataManagerTest::DidGetResponse,
                   base::Unretained(this), run_loop.QuitClosure());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchDataManager::GetJobResponse,
                   base::Unretained(background_fetch_data_manager()), job_guid_,
                   base::Bind(&BackgroundFetchDataManagerTest::DidGetResponse,
                              base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
  }

  void DidGetResponse(base::Closure closure,
                      std::vector<ServiceWorkerResponse> responses,
                      std::vector<std::unique_ptr<BlobHandle>> blobs) {
    responses_ = std::move(responses);
    blobs_ = std::move(blobs);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, closure);
  }

  const std::string& job_guid() const { return job_guid_; }
  BackgroundFetchDataManager* background_fetch_data_manager() {
    return background_fetch_data_manager_.get();
  }

  const std::vector<ServiceWorkerResponse>& responses() const {
    return responses_;
  }
  const std::vector<std::unique_ptr<BlobHandle>>& blobs() const {
    return blobs_;
  }

 private:
  void DidCreateRegistration(
      base::Closure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      std::vector<BackgroundFetchRequestInfo>* out_initial_requests,
      blink::mojom::BackgroundFetchError error,
      std::vector<BackgroundFetchRequestInfo> initial_requests) {
    *out_error = error;
    *out_initial_requests = std::move(initial_requests);

    quit_closure.Run();
  }

  void DidDeleteRegistration(base::Closure quit_closure,
                             blink::mojom::BackgroundFetchError* out_error,
                             blink::mojom::BackgroundFetchError error) {
    *out_error = error;

    quit_closure.Run();
  }

  std::string job_guid_;
  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;
  std::vector<ServiceWorkerResponse> responses_;
  std::vector<std::unique_ptr<BlobHandle>> blobs_;
};

TEST_F(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration that's already known to the system.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kTag, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;
  std::vector<BackgroundFetchRequestInfo> initial_requests;

  // Deleting the not-yet-created registration should fail.
  ASSERT_NO_FATAL_FAILURE(DeleteRegistration(registration_id, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_TAG);

  // Creating the initial registration should succeed.
  ASSERT_NO_FATAL_FAILURE(CreateRegistration(registration_id, requests, options,
                                             &error, &initial_requests));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Attempting to create it again should yield an error.
  ASSERT_NO_FATAL_FAILURE(CreateRegistration(registration_id, requests, options,
                                             &error, &initial_requests));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_TAG);

  // Deleting the registration should succeed.
  ASSERT_NO_FATAL_FAILURE(DeleteRegistration(registration_id, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And then recreating the registration again should work fine.
  ASSERT_NO_FATAL_FAILURE(CreateRegistration(registration_id, requests, options,
                                             &error, &initial_requests));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, OutOfOrderCompletion) {
  CreateRequests(10);
  BackgroundFetchDataManager* data_manager = background_fetch_data_manager();
  const std::string& job_guid = BackgroundFetchDataManagerTest::job_guid();
  std::vector<std::string> request_guids;

  // Start half of the fetch requests.
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(data_manager->HasRequestsRemaining(job_guid));
    const BackgroundFetchRequestInfo& request_info =
        data_manager->GetNextBackgroundFetchRequestInfo(job_guid);
    request_guids.push_back(request_info.guid());
  }

  // Complete all of the fetches out of order except for #1.
  DownloadItem::DownloadState complete = DownloadItem::DownloadState::COMPLETE;
  DownloadInterruptReason no_interrupt =
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
  EXPECT_TRUE(data_manager->UpdateRequestState(job_guid, request_guids[3],
                                               complete, no_interrupt));
  EXPECT_TRUE(data_manager->UpdateRequestState(job_guid, request_guids[2],
                                               complete, no_interrupt));
  EXPECT_TRUE(data_manager->UpdateRequestState(job_guid, request_guids[4],
                                               complete, no_interrupt));
  EXPECT_TRUE(data_manager->UpdateRequestState(job_guid, request_guids[0],
                                               complete, no_interrupt));

  EXPECT_TRUE(data_manager->HasRequestsRemaining(job_guid));
  EXPECT_FALSE(data_manager->IsComplete(job_guid));

  // Start and complete the remaining requests.
  for (int i = 5; i < 10; i++) {
    const BackgroundFetchRequestInfo& request_info =
        data_manager->GetNextBackgroundFetchRequestInfo(job_guid);
    request_guids.push_back(request_info.guid());
    data_manager->UpdateRequestState(job_guid, request_guids[i], complete,
                                     no_interrupt);
  }

  EXPECT_FALSE(data_manager->IsComplete(job_guid));
  EXPECT_FALSE(data_manager->HasRequestsRemaining(job_guid));

  // Complete the final request.
  EXPECT_FALSE(data_manager->UpdateRequestState(job_guid, request_guids[1],
                                                complete, no_interrupt));
  EXPECT_TRUE(data_manager->IsComplete(job_guid));
}

TEST_F(BackgroundFetchDataManagerTest, PauseAndResume) {
  CreateRequests(1);
  BackgroundFetchDataManager* data_manager = background_fetch_data_manager();
  const std::string& job_guid = BackgroundFetchDataManagerTest::job_guid();

  // Start the request.
  ASSERT_TRUE(data_manager->HasRequestsRemaining(job_guid));
  const BackgroundFetchRequestInfo& request_info =
      data_manager->GetNextBackgroundFetchRequestInfo(job_guid);

  EXPECT_FALSE(data_manager->HasRequestsRemaining(job_guid));
  EXPECT_FALSE(data_manager->IsComplete(job_guid));

  // Set the request state to be paused. This should not complete the job.
  EXPECT_FALSE(data_manager->UpdateRequestState(
      job_guid, request_info.guid(), DownloadItem::DownloadState::INTERRUPTED,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN));
  EXPECT_FALSE(data_manager->IsComplete(job_guid));

  // Unpause the request.
  EXPECT_FALSE(data_manager->UpdateRequestState(
      job_guid, request_info.guid(), DownloadItem::DownloadState::IN_PROGRESS,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_FALSE(data_manager->IsComplete(job_guid));

  // Complete the request.
  EXPECT_FALSE(data_manager->UpdateRequestState(
      job_guid, request_info.guid(), DownloadItem::DownloadState::COMPLETE,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_TRUE(data_manager->IsComplete(job_guid));
}

TEST_F(BackgroundFetchDataManagerTest, CancelledRequest) {
  CreateRequests(1);
  BackgroundFetchDataManager* data_manager = background_fetch_data_manager();
  const std::string& job_guid = BackgroundFetchDataManagerTest::job_guid();

  // Start the request.
  ASSERT_TRUE(data_manager->HasRequestsRemaining(job_guid));
  const BackgroundFetchRequestInfo& request_info =
      data_manager->GetNextBackgroundFetchRequestInfo(job_guid);

  EXPECT_FALSE(data_manager->HasRequestsRemaining(job_guid));
  EXPECT_FALSE(data_manager->IsComplete(job_guid));

  // Cancel the request.
  EXPECT_FALSE(data_manager->UpdateRequestState(
      job_guid, request_info.guid(), DownloadItem::DownloadState::CANCELLED,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_TRUE(data_manager->IsComplete(job_guid));
}

TEST_F(BackgroundFetchDataManagerTest, PrepareResponse) {
  CreateRequests(1);
  BackgroundFetchDataManager* data_manager = background_fetch_data_manager();
  const std::string& job_guid = BackgroundFetchDataManagerTest::job_guid();

  const BackgroundFetchRequestInfo& request_info =
      data_manager->GetNextBackgroundFetchRequestInfo(job_guid);
  EXPECT_FALSE(data_manager->UpdateRequestState(
      job_guid, request_info.guid(), DownloadItem::DownloadState::COMPLETE,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_TRUE(data_manager->IsComplete(job_guid));
}

}  // namespace content
