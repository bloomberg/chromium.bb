// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
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

namespace content {

namespace {

const char kOrigin[] = "https://example.com/";
const char kJobGuid[] = "TestRequestGuid";
constexpr int64_t kServiceWorkerRegistrationId = 9001;
const char kTestUrl[] = "http://www.example.com/example.html";
const char kTag[] = "testTag";
const base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("path/file.txt");

}  // namespace

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

class FakeBackgroundFetchDataManager : public BackgroundFetchDataManager {
 public:
  FakeBackgroundFetchDataManager(BrowserContext* browser_context)
      : BackgroundFetchDataManager(browser_context) {}
  ~FakeBackgroundFetchDataManager() = default;

  void set_is_complete(bool is_complete) { is_complete_ = is_complete; }

  void set_next_request(BackgroundFetchRequestInfo* request) {
    next_request_ = request;
  }

  bool UpdateRequestState(const std::string& job_guid,
                          const std::string& request_guid,
                          DownloadItem::DownloadState state,
                          DownloadInterruptReason interrupt_reason) override {
    if (state == DownloadItem::DownloadState::COMPLETE ||
        state == DownloadItem::DownloadState::CANCELLED) {
      if (!next_request_)
        is_complete_ = true;
    }
    current_request_->set_state(state);
    current_request_->set_interrupt_reason(interrupt_reason);
    return next_request_;
  }

  void UpdateRequestDownloadGuid(const std::string& job_guid,
                                 const std::string& request_guid,
                                 const std::string& download_guid) override {
    current_request_->set_download_guid(download_guid);
  }

  void UpdateRequestStorageState(const std::string& job_guid,
                                 const std::string& request_guid,
                                 const base::FilePath& file_path,
                                 int64_t received_bytes) override {
    current_request_->set_file_path(file_path);
    current_request_->set_received_bytes(received_bytes);
  }

  const BackgroundFetchRequestInfo& GetNextBackgroundFetchRequestInfo(
      const std::string& job_guid) override {
    current_request_ = next_request_;
    next_request_ = nullptr;
    return *current_request_;
  }

  bool HasRequestsRemaining(const std::string& job_guid) const override {
    return next_request_;
  }
  bool IsComplete(const std::string& job_guid) const override {
    return is_complete_;
  }

 private:
  bool is_complete_ = false;
  BackgroundFetchRequestInfo* current_request_ = nullptr;
  BackgroundFetchRequestInfo* next_request_ = nullptr;
};

class BackgroundFetchJobControllerTest : public ::testing::Test {
 public:
  BackgroundFetchJobControllerTest()
      : data_manager_(base::MakeUnique<FakeBackgroundFetchDataManager>(
            &browser_context_)),
        download_manager_(new MockDownloadManagerWithCallback()) {}
  ~BackgroundFetchJobControllerTest() override = default;

  void SetUp() override {
    // The download_manager_ ownership is given to the BrowserContext, and the
    // BrowserContext will take care of deallocating it.
    BrowserContext::SetDownloadManagerForTesting(&browser_context_,
                                                 download_manager_);
  }

  void TearDown() override { job_controller_->Shutdown(); }

  void InitializeJobController() {
    job_controller_ = base::MakeUnique<BackgroundFetchJobController>(
        kJobGuid, &browser_context_,
        BrowserContext::GetDefaultStoragePartition(&browser_context_),
        data_manager_.get(),
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

  FakeBackgroundFetchDataManager* data_manager() const {
    return data_manager_.get();
  }

 private:
  bool did_complete_job_ = false;
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<FakeBackgroundFetchDataManager> data_manager_;
  std::unique_ptr<BackgroundFetchJobController> job_controller_;
  MockDownloadManagerWithCallback* download_manager_;
};

TEST_F(BackgroundFetchJobControllerTest, SingleRequestJob) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  BackgroundFetchRequestInfo request_info(GURL(kTestUrl), kJobGuid);
  request_info.set_state(DownloadItem::DownloadState::IN_PROGRESS);
  data_manager()->set_next_request(&request_info);
  InitializeJobController();

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(1);

  StartProcessing();

  // Get the pending download from the download manager.
  auto& download_items = download_manager()->download_items();
  ASSERT_EQ(1U, download_items.size());
  FakeDownloadItem* item = download_items[0].get();

  // Update the observer with no actual change.
  ItemObserver()->OnDownloadUpdated(item);
  EXPECT_EQ(DownloadItem::DownloadState::IN_PROGRESS, request_info.state());
  EXPECT_FALSE(did_complete_job());

  // Update the item to be completed then update the observer. The JobController
  // should update the JobData that the request is complete.
  item->SetState(DownloadItem::DownloadState::COMPLETE);
  ItemObserver()->OnDownloadUpdated(item);

  EXPECT_EQ(DownloadItem::DownloadState::COMPLETE, request_info.state());
  EXPECT_TRUE(did_complete_job());
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  std::vector<BackgroundFetchRequestInfo> request_infos;
  for (int i = 0; i < 10; i++) {
    request_infos.emplace_back(GURL(kTestUrl), base::IntToString(i));
  }
  data_manager()->set_next_request(&request_infos[0]);
  InitializeJobController();

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
  EXPECT_EQ(DownloadItem::DownloadState::IN_PROGRESS, request_infos[0].state());
  ASSERT_EQ(1U, download_items.size());

  for (size_t i = 0; i < 9; i++) {
    // Update the FakeDataManager with the results we expect.
    if (i < 9)
      data_manager()->set_next_request(&request_infos[i + 1]);

    // Update the next item to be completed then update the observer.
    ASSERT_EQ(i + 1, download_items.size());
    item = download_items[i].get();
    item->SetState(DownloadItem::DownloadState::COMPLETE);
    ItemObserver()->OnDownloadUpdated(item);
    EXPECT_EQ(DownloadItem::DownloadState::COMPLETE, request_infos[i].state());
  }
  EXPECT_FALSE(did_complete_job());

  // Finally, update the last request to be complete. The JobController should
  // see that there are no more requests and mark the job as done.
  ASSERT_EQ(10U, download_items.size());
  item = download_items[9].get();
  item->SetState(DownloadItem::DownloadState::COMPLETE);
  ItemObserver()->OnDownloadUpdated(item);

  EXPECT_TRUE(did_complete_job());
}

TEST_F(BackgroundFetchJobControllerTest, UpdateStorageState) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  BackgroundFetchRequestInfo request_info(GURL(kTestUrl), kJobGuid);
  request_info.set_state(DownloadItem::DownloadState::IN_PROGRESS);
  data_manager()->set_next_request(&request_info);
  InitializeJobController();

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(1);

  StartProcessing();

  // Get the pending download from the download manager.
  auto& download_items = download_manager()->download_items();
  ASSERT_EQ(1U, download_items.size());
  FakeDownloadItem* item = download_items[0].get();

  item->SetTargetFilePath(base::FilePath(kFileName));
  item->SetReceivedBytes(123);
  item->SetState(DownloadItem::DownloadState::COMPLETE);

  // Trigger the observer. The JobController should update the JobData that the
  // request is complete and should fill in storage state.
  ItemObserver()->OnDownloadUpdated(item);

  EXPECT_EQ(123, request_info.received_bytes());
  EXPECT_TRUE(data_manager()->IsComplete(kJobGuid));
  EXPECT_TRUE(did_complete_job());
}

}  // namespace content
