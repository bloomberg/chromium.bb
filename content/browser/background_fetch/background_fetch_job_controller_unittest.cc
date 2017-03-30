// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace content {
namespace {

const char kExampleTag[] = "my-example-tag";

// Use the basic MockDownloadManager, but override it so that it implements the
// functionality that the JobController requires.
class MockDownloadManagerWithCallback : public MockDownloadManager {
 public:
  MockDownloadManagerWithCallback() = default;
  ~MockDownloadManagerWithCallback() override = default;

  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override {
    DownloadUrlMock(params.get());

    auto download_item = base::MakeUnique<FakeDownloadItem>();
    download_item->SetState(DownloadItem::DownloadState::IN_PROGRESS);
    download_item->SetGuid(base::GenerateGUID());

    // Post a task to invoke the callback included with the |params|. This is
    // done asynchronously to match the semantics of the download manager.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(params->callback(), download_item.get(),
                                       DOWNLOAD_INTERRUPT_REASON_NONE));

    // Post a task for automatically completing the download item if requested.
    if (post_completion_task_enabled_) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&MockDownloadManagerWithCallback::MarkAsCompleted,
                     base::Unretained(this), download_item.get()));
    }

    download_items_.insert(
        std::make_pair(download_item->GetGuid(), std::move(download_item)));
  }

  DownloadItem* GetDownloadByGuid(const std::string& guid) override {
    auto iter = download_items_.find(guid);
    if (iter != download_items_.end())
      return iter->second.get();

    return nullptr;
  }

  // Returns a vector with pointers to all downloaded items. The lifetime of the
  // returned pointers is scoped to the lifetime of this instance, which will be
  // kept alive for the lifetime of the tests.
  std::vector<FakeDownloadItem*> GetDownloadItems() {
    std::vector<FakeDownloadItem*> download_items;
    for (const auto& pair : download_items_)
      download_items.push_back(pair.second.get());

    return download_items;
  }

  // Will automatically mark downloads as completing shortly after they start.
  void set_post_completion_task_enabled(bool enabled) {
    post_completion_task_enabled_ = enabled;
  }

 private:
  // Marks the |download_item| as having completed.
  void MarkAsCompleted(FakeDownloadItem* download_item) {
    download_item->SetState(DownloadItem::DownloadState::COMPLETE);
    download_item->NotifyDownloadUpdated();
  }

  // All ever-created download items for this manager. Owned by this instance.
  std::unordered_map<std::string, std::unique_ptr<FakeDownloadItem>>
      download_items_;

  // Whether a task should be posted for automatically marking the download as
  // having completed, avoiding complicated spinning for tests that don't care.
  bool post_completion_task_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadManagerWithCallback);
};

class BackgroundFetchJobControllerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchJobControllerTest()
      : data_manager_(browser_context()), download_manager_(nullptr) {}
  ~BackgroundFetchJobControllerTest() override = default;

  void SetUp() override {
    BackgroundFetchTestBase::SetUp();

    download_manager_ = new MockDownloadManagerWithCallback();

    // The download_manager_ ownership is given to the BrowserContext, and the
    // BrowserContext will take care of deallocating it.
    BrowserContext::SetDownloadManagerForTesting(browser_context(),
                                                 download_manager_);
  }

  // Creates a new Background Fetch registration, whose id will be stored in
  // the |*registration_id|, and registers it with the DataManager for the
  // included |request_data|. Should be wrapped in ASSERT_NO_FATAL_FAILURE().
  void CreateRegistrationForRequests(
      BackgroundFetchRegistrationId* registration_id,
      std::vector<BackgroundFetchRequestInfo>* out_initial_requests,
      std::map<std::string /* url */, std::string /* method */> request_data) {
    DCHECK(registration_id);
    DCHECK(out_initial_requests);

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
                       base::Unretained(this), &error, out_initial_requests,
                       run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_GE(out_initial_requests->size(), 1u);
    ASSERT_LE(out_initial_requests->size(),
              kMaximumBackgroundFetchParallelRequests);
  }

  // Creates a new BackgroundFetchJobController instance.
  std::unique_ptr<BackgroundFetchJobController> CreateJobController(
      const BackgroundFetchRegistrationId& registration_id) {
    return base::MakeUnique<BackgroundFetchJobController>(
        registration_id, BackgroundFetchOptions(), browser_context(),
        BrowserContext::GetDefaultStoragePartition(browser_context()),
        &data_manager_,
        base::BindOnce(&BackgroundFetchJobControllerTest::DidCompleteJob,
                       base::Unretained(this)));
  }

 protected:
  BackgroundFetchDataManager data_manager_;
  MockDownloadManagerWithCallback* download_manager_;  // BrowserContext owned
  bool did_complete_job_ = false;

  // Closure that will be invoked when the JobController has completed all
  // available jobs. Enables use of a run loop for deterministic waits.
  base::OnceClosure job_completed_closure_;

 private:
  void DidCreateRegistration(
      blink::mojom::BackgroundFetchError* out_error,
      std::vector<BackgroundFetchRequestInfo>* out_initial_requests,
      const base::Closure& quit_closure,
      blink::mojom::BackgroundFetchError error,
      std::vector<BackgroundFetchRequestInfo> initial_requests) {
    DCHECK(out_error);
    DCHECK(out_initial_requests);

    *out_error = error;
    *out_initial_requests = std::move(initial_requests);

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
  std::vector<BackgroundFetchRequestInfo> initial_requests;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, &initial_requests,
      {{"https://example.com/funny_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);
  EXPECT_CALL(*download_manager_, DownloadUrlMock(_)).Times(1);

  controller->Start(initial_requests /* deliberate copy */);
  EXPECT_EQ(controller->state(), BackgroundFetchJobController::State::FETCHING);

  // Allows for proper initialization of the FakeDownloadItems.
  base::RunLoop().RunUntilIdle();

  std::vector<FakeDownloadItem*> download_items =
      download_manager_->GetDownloadItems();
  ASSERT_EQ(download_items.size(), 1u);

  // Mark the single download item as finished, completing the job.
  {
    FakeDownloadItem* item = download_items[0];

    EXPECT_EQ(DownloadItem::DownloadState::IN_PROGRESS, item->GetState());

    item->SetState(DownloadItem::DownloadState::COMPLETE);
    item->NotifyDownloadUpdated();
  }

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::COMPLETED);
  EXPECT_TRUE(did_complete_job_);
}

TEST_F(BackgroundFetchJobControllerTest, MultipleRequestJob) {
  BackgroundFetchRegistrationId registration_id;
  std::vector<BackgroundFetchRequestInfo> initial_requests;

  // This test should always issue more requests than the number of allowed
  // parallel requests. That way we ensure testing the iteration behaviour.
  ASSERT_GT(5u, kMaximumBackgroundFetchParallelRequests);

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, &initial_requests,
      {{"https://example.com/funny_cat.png", "GET"},
       {"https://example.com/scary_cat.png", "GET"},
       {"https://example.com/crazy_cat.png", "GET"},
       {"https://example.com/silly_cat.png", "GET"},
       {"https://example.com/happy_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);

  // Enable automatically marking downloads as finished.
  download_manager_->set_post_completion_task_enabled(true);

  // Continue spinning until the Job Controller has completed all the requests.
  // The Download Manager has been told to automatically mark them as finished.
  {
    base::RunLoop run_loop;
    job_completed_closure_ = run_loop.QuitClosure();

    EXPECT_CALL(*download_manager_, DownloadUrlMock(_)).Times(5);

    controller->Start(initial_requests /* deliberate copy */);
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
  std::vector<BackgroundFetchRequestInfo> initial_requests;

  ASSERT_NO_FATAL_FAILURE(CreateRegistrationForRequests(
      &registration_id, &initial_requests,
      {{"https://example.com/sad_cat.png", "GET"}}));

  std::unique_ptr<BackgroundFetchJobController> controller =
      CreateJobController(registration_id);

  EXPECT_EQ(controller->state(),
            BackgroundFetchJobController::State::INITIALIZED);
  EXPECT_CALL(*download_manager_, DownloadUrlMock(_)).Times(1);

  controller->Start(initial_requests /* deliberate copy */);
  EXPECT_EQ(controller->state(), BackgroundFetchJobController::State::FETCHING);

  controller->Abort();

  // TODO(peter): Verify that the issued download items have had their state
  // updated to be cancelled as well.

  EXPECT_EQ(controller->state(), BackgroundFetchJobController::State::ABORTED);
  EXPECT_TRUE(did_complete_job_);
}

}  // namespace
}  // namespace content
