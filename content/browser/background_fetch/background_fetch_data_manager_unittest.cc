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
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kResource[] = "https://example.com/resource.html";
const char kTag[] = "TestRequestTag";
const char kJobOrigin[] = "https://example.com";
const int64_t kServiceWorkerRegistrationId = 9001;

}  // namespace

class BackgroundFetchDataManagerTest : public ::testing::Test {
 public:
  BackgroundFetchDataManagerTest()
      : background_fetch_data_manager_(
            base::MakeUnique<BackgroundFetchDataManager>(&browser_context_)) {}
  ~BackgroundFetchDataManagerTest() override = default;

 protected:
  void CreateRequests(int num_requests) {
    DCHECK(num_requests > 0);
    // Create |num_requests| BackgroundFetchRequestInfo's.
    std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos;
    for (int i = 0; i < num_requests; i++) {
      request_infos.push_back(
          base::MakeUnique<BackgroundFetchRequestInfo>(GURL(kResource), kTag));
    }
    std::unique_ptr<BackgroundFetchJobInfo> job_info =
        base::MakeUnique<BackgroundFetchJobInfo>(
            "tag", url::Origin(GURL(kJobOrigin)), kServiceWorkerRegistrationId);
    job_guid_ = job_info->guid();
    background_fetch_data_manager_->CreateRequest(std::move(job_info),
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
  std::string job_guid_;
  TestBrowserContext browser_context_;
  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;
  TestBrowserThreadBundle thread_bundle_;
  std::vector<ServiceWorkerResponse> responses_;
  std::vector<std::unique_ptr<BlobHandle>> blobs_;
};

TEST_F(BackgroundFetchDataManagerTest, CompleteJob) {
  CreateRequests(10);
  BackgroundFetchDataManager* data_manager = background_fetch_data_manager();
  std::vector<std::string> request_guids;

  // Get all of the fetch requests from the BackgroundFetchDataManager.
  for (int i = 0; i < 10; i++) {
    EXPECT_FALSE(data_manager->IsComplete(job_guid()));
    ASSERT_TRUE(data_manager->HasRequestsRemaining(job_guid()));
    const BackgroundFetchRequestInfo& request_info =
        data_manager->GetNextBackgroundFetchRequestInfo(job_guid());
    request_guids.push_back(request_info.guid());
    EXPECT_EQ(request_info.tag(), kTag);
    EXPECT_EQ(request_info.state(),
              DownloadItem::DownloadState::MAX_DOWNLOAD_STATE);
    EXPECT_EQ(request_info.interrupt_reason(),
              DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE);
  }

  // At this point, all the fetches have been started, but none finished.
  EXPECT_FALSE(data_manager->HasRequestsRemaining(job_guid()));

  // Complete all of the fetch requests.
  for (int i = 0; i < 10; i++) {
    EXPECT_FALSE(data_manager->IsComplete(job_guid()));
    EXPECT_FALSE(data_manager->UpdateRequestState(
        job_guid(), request_guids[i], DownloadItem::DownloadState::COMPLETE,
        DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  }

  // All requests are complete now.
  EXPECT_TRUE(data_manager->IsComplete(job_guid()));
  GetResponse();

  EXPECT_EQ(10U, responses().size());
  EXPECT_EQ(10U, blobs().size());
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
