// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/save_page_request.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kRequestId = 7;
const GURL kHttpUrl("http://tunafish.com");
const ClientId kClientId("AsyncLoading", "88");

// Mock Loader for testing the Offliner calls.
class MockPrerenderingLoader : public PrerenderingLoader {
 public:
  explicit MockPrerenderingLoader(content::BrowserContext* browser_context)
      : PrerenderingLoader(browser_context),
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
  bool IsIdle() override { return !mock_loading_ && !mock_loaded_; }
  bool IsLoaded() override { return mock_loaded_; }

  void CallbackForLoadFailed() {
    DCHECK(mock_loading_);
    mock_loading_ = false;
    mock_loaded_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(load_page_callback_, Offliner::RequestStatus::FAILED,
                   nullptr /* web_contents */));
  }

  bool mock_loading() const { return mock_loading_; }
  bool mock_loaded() const { return mock_loaded_; }
  const LoadPageCallback& load_page_callback() { return load_page_callback_; }

 private:
  bool mock_loading_;
  bool mock_loaded_;
  LoadPageCallback load_page_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockPrerenderingLoader);
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

  bool loading() const { return loader_->mock_loading(); }
  bool loaded() const { return loader_->mock_loaded(); }
  MockPrerenderingLoader* loader() { return loader_; }
  bool callback_called() { return callback_called_; }
  Offliner::RequestStatus request_status() { return request_status_; }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::RequestStatus status);

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<PrerenderingOffliner> offliner_;
  // Not owned.
  MockPrerenderingLoader* loader_;
  bool callback_called_;
  Offliner::RequestStatus request_status_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOfflinerTest);
};

PrerenderingOfflinerTest::PrerenderingOfflinerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      callback_called_(false),
      request_status_(Offliner::RequestStatus::UNKNOWN) {}

PrerenderingOfflinerTest::~PrerenderingOfflinerTest() {}

void PrerenderingOfflinerTest::SetUp() {
  offliner_.reset(new PrerenderingOffliner(nullptr, nullptr, nullptr));
  std::unique_ptr<MockPrerenderingLoader> mock_loader(
      new MockPrerenderingLoader(nullptr));
  loader_ = mock_loader.get();
  offliner_->SetLoaderForTesting(std::move(mock_loader));
}

void PrerenderingOfflinerTest::OnCompletion(const SavePageRequest& request,
                                            Offliner::RequestStatus status) {
  DCHECK(!callback_called_);  // Only expect single callback per request.
  callback_called_ = true;
  request_status_ = status;
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadStartedButFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loading());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CallbackForLoadFailed();
  PumpLoop();
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(Offliner::RequestStatus::FAILED, request_status());
  EXPECT_FALSE(loading());
  EXPECT_FALSE(loaded());
}

TEST_F(PrerenderingOfflinerTest, CancelWhenLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loading());

  offliner()->Cancel();
  EXPECT_FALSE(loading());
}

}  // namespace offline_pages
