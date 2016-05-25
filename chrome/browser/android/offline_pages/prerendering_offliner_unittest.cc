// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/save_page_request.h"
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
      : PrerenderingLoader(browser_context), mock_loading_(false) {}
  ~MockPrerenderingLoader() override {}

  bool LoadPage(const GURL& url, const LoadPageCallback& callback) override {
    mock_loading_ = true;
    return mock_loading_;
  }

  void StopLoading() override { mock_loading_ = false; }

  bool mock_loading() const { return mock_loading_; }

 private:
  bool mock_loading_;

  DISALLOW_COPY_AND_ASSIGN(MockPrerenderingLoader);
};

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
  Offliner::CompletionStatus completion_status() {
    return completion_status_;
  }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::CompletionStatus status);

  std::unique_ptr<PrerenderingOffliner> offliner_;
  // Not owned.
  MockPrerenderingLoader* loader_;
  Offliner::CompletionStatus completion_status_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOfflinerTest);
};

PrerenderingOfflinerTest::PrerenderingOfflinerTest() {}

PrerenderingOfflinerTest::~PrerenderingOfflinerTest() {}

void PrerenderingOfflinerTest::SetUp() {
  offliner_.reset(new PrerenderingOffliner(nullptr, nullptr, nullptr));
  std::unique_ptr<MockPrerenderingLoader> mock_loader(
      new MockPrerenderingLoader(nullptr));
  loader_ = mock_loader.get();
  offliner_->SetLoaderForTesting(std::move(mock_loader));
}

void PrerenderingOfflinerTest::OnCompletion(const SavePageRequest& request,
                                            Offliner::CompletionStatus status) {
  completion_status_ = status;
}

// Tests initiate loading.
TEST_F(PrerenderingOfflinerTest, LoadAndSaveBadUrl) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kFileUrl, kClientId, creation_time);
  EXPECT_FALSE(offliner()->LoadAndSave(request, callback()));
  EXPECT_FALSE(loading());
}

// Tests initiate loading.
TEST_F(PrerenderingOfflinerTest, LoadAndSaveStartsLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loading());
}

// Tests cancels loading.
TEST_F(PrerenderingOfflinerTest, CancelLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time);
  EXPECT_TRUE(offliner()->LoadAndSave(request, callback()));
  EXPECT_TRUE(loading());

  offliner()->Cancel();
  EXPECT_FALSE(loading());
}

}  // namespace offline_pages
