// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/stub_offline_page_model.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kRequestId = 7;
const GURL kHttpUrl("http://tunafish.com");
const GURL kFileUrl("file://sailfish.png");
const ClientId kClientId("AsyncLoading", "88");

// Mock Loader for testing the Offliner calls.
class MockPrerenderingLoader : public PrerenderingLoader {
 public:
  explicit MockPrerenderingLoader(content::BrowserContext* browser_context)
      : PrerenderingLoader(browser_context),
        can_prerender_(true),
        mock_loading_(false),
        mock_loaded_(false) {}
  ~MockPrerenderingLoader() override {}

  bool LoadPage(const GURL& url, const LoadPageCallback& callback) override {
    mock_loading_ = true;
    load_page_callback_ = callback;
    return mock_loading_;
  }

  void StopLoading() override {
    mock_loading_ = false;
    mock_loaded_ = false;
  }

  bool CanPrerender() override { return can_prerender_; }
  bool IsIdle() override { return !mock_loading_ && !mock_loaded_; }
  bool IsLoaded() override { return mock_loaded_; }

  void CompleteLoadingAsFailed() {
    DCHECK(mock_loading_);
    mock_loading_ = false;
    mock_loaded_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(load_page_callback_, Offliner::RequestStatus::FAILED,
                   nullptr /* web_contents */));
  }

  void CompleteLoadingAsLoaded() {
    DCHECK(mock_loading_);
    mock_loading_ = false;
    mock_loaded_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(load_page_callback_, Offliner::RequestStatus::LOADED,
                   content::WebContentsTester::CreateTestWebContents(
                       new TestingProfile(), NULL)));
  }

  void CompleteLoadingAsCanceled() {
    DCHECK(!IsIdle());
    mock_loading_ = false;
    mock_loaded_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(load_page_callback_, Offliner::RequestStatus::CANCELED,
                   nullptr /* web_contents */));
  }

  void DisablePrerendering() { can_prerender_ = false; }
  const LoadPageCallback& load_page_callback() { return load_page_callback_; }

 private:
  bool can_prerender_;
  bool mock_loading_;
  bool mock_loaded_;
  LoadPageCallback load_page_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockPrerenderingLoader);
};

// Mock OfflinePageModel for testing the SavePage calls.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel() : mock_saving_(false) {}
  ~MockOfflinePageModel() override {}

  void SavePage(const GURL& url,
                const ClientId& client_id,
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

class PrerenderingOfflinerTest : public testing::Test {
 public:
  PrerenderingOfflinerTest();
  ~PrerenderingOfflinerTest() override;

  void SetUp() override;

  PrerenderingOffliner* offliner() const { return offliner_.get(); }
  Offliner::CompletionCallback const callback() {
    return base::Bind(&PrerenderingOfflinerTest::OnCompletion,
                      base::Unretained(this));
  }

  bool SaveInProgress() const { return model_->mock_saving(); }
  MockPrerenderingLoader* loader() { return loader_; }
  MockOfflinePageModel* model() { return model_; }
  bool completion_callback_called() { return completion_callback_called_; }
  Offliner::RequestStatus request_status() { return request_status_; }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::RequestStatus status);

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<PrerenderingOffliner> offliner_;
  // Not owned.
  MockPrerenderingLoader* loader_;
  MockOfflinePageModel* model_;
  bool completion_callback_called_;
  Offliner::RequestStatus request_status_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOfflinerTest);
};

PrerenderingOfflinerTest::PrerenderingOfflinerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      completion_callback_called_(false),
      request_status_(Offliner::RequestStatus::UNKNOWN) {}

PrerenderingOfflinerTest::~PrerenderingOfflinerTest() {}

void PrerenderingOfflinerTest::SetUp() {
  model_ = new MockOfflinePageModel();
  offliner_.reset(new PrerenderingOffliner(nullptr, nullptr, model_));
  std::unique_ptr<MockPrerenderingLoader> mock_loader(
      new MockPrerenderingLoader(nullptr));
  loader_ = mock_loader.get();
  offliner_->SetLoaderForTesting(std::move(mock_loader));
}

void PrerenderingOfflinerTest::OnCompletion(const SavePageRequest& request,
                                            Offliner::RequestStatus status) {
  DCHECK(!completion_callback_called_);  // Expect single callback per request.
  completion_callback_called_ = true;
  request_status_ = status;
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveBadUrl) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kFileUrl, kClientId, creation_time);
  EXPECT_FALSE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSavePrerenderingDisabled) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  loader()->DisablePrerendering();
  EXPECT_FALSE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadStartedButFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsFailed();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::FAILED, request_status());
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, CancelWhenLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());

  offliner()->Cancel();
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, CancelWhenLoaded) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  offliner()->Cancel();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_FALSE(loader()->IsLoaded());
  // Note: save still in progress since it does not support canceling.
  EXPECT_TRUE(SaveInProgress());

  // Subsequent save callback causes no harm (no crash and no callback).
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadedButSaveFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::FAILED_SAVE, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveSuccessful) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadedButThenCanceledFromLoader) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  loader()->CompleteLoadingAsCanceled();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::CANCELED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  // Note: save still in progress since it does not support canceling.
  EXPECT_TRUE(SaveInProgress());
}

}  // namespace offline_pages
