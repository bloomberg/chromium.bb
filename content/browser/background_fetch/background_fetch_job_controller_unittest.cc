// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "https://example.com/";
const char kJobGuid[] = "TestRequestGuid";
constexpr int64_t kServiceWorkerRegistrationId = 9001;
const char kTestUrl[] = "http://www.example.com/example.html";
const char kTag[] = "testTag";

}  // namespace

namespace content {

// Use the basic MockDownloadManager, but override it so that it implements the
// functionality that the JobController requires.
class MockDownloadManagerWithCallback : public MockDownloadManager {
 public:
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override {
    base::RunLoop run_loop;
    DownloadUrlMock(params.get());
    std::string guid = base::GenerateGUID();
    std::unique_ptr<FakeDownloadItem> item =
        base::MakeUnique<FakeDownloadItem>();
    item->SetState(DownloadItem::DownloadState::IN_PROGRESS);
    item->SetGuid(guid);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(params->callback(), item.get(),
                                       DOWNLOAD_INTERRUPT_REASON_NONE));
    download_items_.push_back(std::move(item));
    run_loop.RunUntilIdle();
  }

  // This is called during shutdown for each DownloadItem so can be an
  // O(n^2) where n is controlled by the users of the API. If n is expected to
  // be larger or the method is called in more places, consider alternatives.
  DownloadItem* GetDownloadByGuid(const std::string& guid) override {
    for (const auto& item : download_items_) {
      if (item->GetGuid() == guid)
        return item.get();
    }
    return nullptr;
  }

  const std::vector<std::unique_ptr<FakeDownloadItem>>& download_items() const {
    return download_items_;
  }

 private:
  std::vector<std::unique_ptr<FakeDownloadItem>> download_items_;
};

class BackgroundFetchJobControllerTest : public ::testing::Test {
 public:
  BackgroundFetchJobControllerTest()
      : download_manager_(new MockDownloadManagerWithCallback()) {}
  ~BackgroundFetchJobControllerTest() override = default;

  void SetUp() override {
    // The download_manager_ ownership is given to the BrowserContext, and the
    // BrowserContext will take care of deallocating it.
    BrowserContext::SetDownloadManagerForTesting(&browser_context_,
                                                 download_manager_);
  }

  void TearDown() override { job_controller_->Shutdown(); }

  void InitializeJobController(
      std::unique_ptr<BackgroundFetchJobData> job_data) {
    job_controller_ = base::MakeUnique<BackgroundFetchJobController>(
        kJobGuid, &browser_context_,
        BrowserContext::GetDefaultStoragePartition(&browser_context_),
        std::move(job_data),
        base::BindOnce(&BackgroundFetchJobControllerTest::DidCompleteJob,
                       base::Unretained(this)));
  }

  void DidCompleteJob() { did_complete_job_ = true; }

  void StartProcessing() {
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchJobControllerTest::StartProcessingOnIO,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void StartProcessingOnIO(const base::Closure& closure) {
    job_controller_->StartProcessing();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, closure);
  }

  BackgroundFetchJobController* job_controller() {
    return job_controller_.get();
  }
  MockDownloadManagerWithCallback* download_manager() {
    return download_manager_;
  }

  DownloadItem::Observer* ItemObserver() const { return job_controller_.get(); }

  bool did_complete_job() const { return did_complete_job_; }

 private:
  bool did_complete_job_ = false;
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<BackgroundFetchJobController> job_controller_;
  MockDownloadManagerWithCallback* download_manager_;
};

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJob) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  BackgroundFetchRequestInfo request_info(GURL(kTestUrl), kJobGuid);
  std::vector<BackgroundFetchRequestInfo> request_infos{request_info};

  // Get a JobData to give to the JobController. The JobController then gets
  // the BackgroundFetchRequestInfos from the JobData.
  std::unique_ptr<BackgroundFetchJobData> owned_job_data =
      base::MakeUnique<BackgroundFetchJobData>(request_infos);
  BackgroundFetchJobData* job_data = owned_job_data.get();
  InitializeJobController(std::move(owned_job_data));

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(1);

  StartProcessing();

  // Get one of the pending downloads from the download manager.
  auto& download_items = download_manager()->download_items();
  ASSERT_EQ(1U, download_items.size());
  FakeDownloadItem* item = download_items[0].get();

  // Update the observer with no actual change.
  ItemObserver()->OnDownloadUpdated(item);
  EXPECT_FALSE(job_data->IsComplete());
  EXPECT_FALSE(did_complete_job());

  // Update the item to be completed then update the observer. The JobController
  // should update the JobData that the request is complete.
  item->SetState(DownloadItem::DownloadState::COMPLETE);
  ItemObserver()->OnDownloadUpdated(item);
  EXPECT_TRUE(job_data->IsComplete());

  EXPECT_TRUE(did_complete_job());
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  std::vector<BackgroundFetchRequestInfo> request_infos;
  for (int i = 0; i < 10; i++) {
    request_infos.emplace_back(GURL(kTestUrl), base::IntToString(i));
  }

  // Get a JobData to give to the JobController. The JobController then gets
  // the BackgroundFetchRequestInfos from the JobData.
  std::unique_ptr<BackgroundFetchJobData> owned_job_data =
      base::MakeUnique<BackgroundFetchJobData>(request_infos);
  BackgroundFetchJobData* job_data = owned_job_data.get();
  InitializeJobController(std::move(owned_job_data));

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(10);

  StartProcessing();

  // Get one of the pending downloads from the download manager.
  auto& download_items = download_manager()->download_items();
  ASSERT_EQ(1U, download_items.size());
  FakeDownloadItem* item = download_items[0].get();

  // Update the observer with no actual change.
  ItemObserver()->OnDownloadUpdated(item);
  EXPECT_FALSE(job_data->IsComplete());
  ASSERT_EQ(1U, download_items.size());

  for (size_t i = 0; i < 9; i++) {
    // Update the next item to be completed then update the observer.
    ASSERT_EQ(i + 1, download_items.size());
    item = download_items[i].get();
    item->SetState(DownloadItem::DownloadState::COMPLETE);
    ItemObserver()->OnDownloadUpdated(item);
    EXPECT_FALSE(job_data->IsComplete());
  }
  EXPECT_FALSE(job_data->HasRequestsRemaining());
  EXPECT_FALSE(did_complete_job());

  // Finally, update the last request to be complete. The JobController should
  // see that there are no more requests and mark the job as done.
  ASSERT_EQ(10U, download_items.size());
  item = download_items[9].get();
  item->SetState(DownloadItem::DownloadState::COMPLETE);
  ItemObserver()->OnDownloadUpdated(item);
  EXPECT_TRUE(job_data->IsComplete());

  EXPECT_TRUE(did_complete_job());
}

}  // namespace content
