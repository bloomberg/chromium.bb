// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/background_loader_offliner.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/content/background_loader/background_loader_contents_stub.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kRequestId = 7;
const GURL kHttpUrl("http://www.tunafish.com");
const GURL kFileUrl("file://salmon.png");
const ClientId kClientId("AsyncLoading", "88");
const bool kUserRequested = true;

// Mock OfflinePageModel for testing the SavePage calls
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel() : mock_saving_(false) {}
  ~MockOfflinePageModel() override {}

  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override {
    mock_saving_ = true;
    save_page_callback_ = callback;
  }

  void CompleteSavingAsArchiveCreationFailed() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(save_page_callback_,
                              SavePageResult::ARCHIVE_CREATION_FAILED, 0));
  }

  void CompleteSavingAsSuccess() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(save_page_callback_, SavePageResult::SUCCESS, 123456));
  }

  bool mock_saving() const { return mock_saving_; }

 private:
  bool mock_saving_;
  SavePageCallback save_page_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockOfflinePageModel);
};

void PumpLoop() {
  base::RunLoop().RunUntilIdle();
}
}  // namespace

// A BackgroundLoader that we can run tests on.
// Overrides the ResetState so we don't actually try to create any web contents.
// This is a temporary solution to test core BackgroundLoaderOffliner
// functionality until we straighten out assumptions made by RequestCoordinator
// so that the ResetState method is no longer needed.
class TestBackgroundLoaderOffliner : public BackgroundLoaderOffliner {
 public:
  explicit TestBackgroundLoaderOffliner(
      content::BrowserContext* browser_context,
      OfflinePageModel* offline_page_model);
  ~TestBackgroundLoaderOffliner() override;
  content::WebContentsTester* web_contents() {
    return content::WebContentsTester::For(stub_->web_contents());
  }

  bool is_loading() { return stub_->is_loading(); }

 protected:
  void ResetState() override;

 private:
  background_loader::BackgroundLoaderContentsStub* stub_;
};

TestBackgroundLoaderOffliner::TestBackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    OfflinePageModel* offline_page_model)
    : BackgroundLoaderOffliner(browser_context, offline_page_model) {}

TestBackgroundLoaderOffliner::~TestBackgroundLoaderOffliner() {}

void TestBackgroundLoaderOffliner::ResetState() {
  pending_request_.reset();
  stub_ = new background_loader::BackgroundLoaderContentsStub(browser_context_);
  loader_.reset(stub_);
  content::WebContentsObserver::Observe(stub_->web_contents());
}

class BackgroundLoaderOfflinerTest : public testing::Test {
 public:
  BackgroundLoaderOfflinerTest();
  ~BackgroundLoaderOfflinerTest() override;

  void SetUp() override;

  TestBackgroundLoaderOffliner* offliner() const { return offliner_.get(); }
  Offliner::CompletionCallback const callback() {
    return base::Bind(&BackgroundLoaderOfflinerTest::OnCompletion,
                      base::Unretained(this));
  }
  Profile* profile() { return &profile_; }
  bool completion_callback_called() { return completion_callback_called_; }
  Offliner::RequestStatus request_status() { return request_status_; }
  bool SaveInProgress() const { return model_->mock_saving(); }
  MockOfflinePageModel* model() const { return model_; }

  void CompleteLoading() {
    // For some reason, setting loading to True will call DidStopLoading
    // on the observers.
    offliner()->web_contents()->TestSetIsLoading(true);
  }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::RequestStatus status);
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<TestBackgroundLoaderOffliner> offliner_;
  MockOfflinePageModel* model_;
  bool completion_callback_called_;
  Offliner::RequestStatus request_status_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoaderOfflinerTest);
};

BackgroundLoaderOfflinerTest::BackgroundLoaderOfflinerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      completion_callback_called_(false),
      request_status_(Offliner::RequestStatus::UNKNOWN) {}

BackgroundLoaderOfflinerTest::~BackgroundLoaderOfflinerTest() {}

void BackgroundLoaderOfflinerTest::SetUp() {
  model_ = new MockOfflinePageModel();
  offliner_.reset(new TestBackgroundLoaderOffliner(profile(), model_));
}

void BackgroundLoaderOfflinerTest::OnCompletion(
    const SavePageRequest& request,
    Offliner::RequestStatus status) {
  DCHECK(!completion_callback_called_);  // Expect 1 callback per request.
  completion_callback_called_ = true;
  request_status_ = status;
}

TEST_F(BackgroundLoaderOfflinerTest, LoadAndSaveStartsLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
  EXPECT_FALSE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, CompleteLoadingInitiatesSave) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  CompleteLoading();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(SaveInProgress());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, CancelWhenLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  offliner()->Cancel();
  EXPECT_FALSE(offliner()->is_loading());  // Offliner reset.
}

TEST_F(BackgroundLoaderOfflinerTest, CancelWhenLoaded) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  CompleteLoading();
  PumpLoop();
  offliner()->Cancel();

  // Subsequent save callback cause no crash.
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_FALSE(SaveInProgress());
  EXPECT_FALSE(offliner()->is_loading());  // Offliner reset.
}

TEST_F(BackgroundLoaderOfflinerTest, LoadedButSaveFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));

  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVE_FAILED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, LoadAndSaveSuccess) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));

  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsSuccess();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnInvalidURL) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kFileUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_FALSE(offliner()->LoadAndSave(request, callback()));
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnRenderCrash) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  offliner()->RenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED_NO_NEXT, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnRenderKilled) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  offliner()->RenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED);

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnWebContentsDestroyed) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  offliner()->WebContentsDestroyed();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED, request_status());
}

}  // namespace offline_pages
