// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_download_item.h"
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

// Use the basic MockDownloadItem, but override it to provide a valid GUID.
class MockDownloadItemWithValues : public MockDownloadItem {
 public:
  const std::string& GetGuid() const override { return guid_; }
  void SetGuid(const std::string& guid) { guid_ = guid; }

 private:
  std::string guid_;
};

// Use the basic MockDownloadManager, but override it so that it implements the
// functionality that the JobController requires.
class MockDownloadManagerWithCallback : public MockDownloadManager {
 public:
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override {
    DownloadUrlMock(params.get());
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(params->callback(), &download_item_,
                                       DOWNLOAD_INTERRUPT_REASON_NONE));
  }

  DownloadItem* GetDownloadByGuid(const std::string& guid) override {
    DCHECK_EQ(download_item_.GetGuid(), guid);
    return &download_item_;
  }

  MockDownloadItemWithValues* download_item() { return &download_item_; }

 private:
  MockDownloadItemWithValues download_item_;
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
        std::move(job_data));
  }

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

 private:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<BackgroundFetchJobController> job_controller_;
  MockDownloadManagerWithCallback* download_manager_;
};

TEST_F(BackgroundFetchJobControllerTest, StartDownload) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  BackgroundFetchRequestInfo request_info(GURL(kTestUrl), kJobGuid);
  std::vector<BackgroundFetchRequestInfo> request_infos{request_info};

  // Create a MockDownloadItem that the test can manipulate.
  MockDownloadItemWithValues* item = download_manager()->download_item();
  item->SetGuid("foo");

  // Get a JobData to give to the JobController. The JobController then gets
  // the BackgroundFetchRequestInfos from the JobData.
  std::unique_ptr<BackgroundFetchJobData> job_data =
      base::MakeUnique<BackgroundFetchJobData>(request_infos);
  InitializeJobController(std::move(job_data));

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(1);

  StartProcessing();
}

}  // namespace content
