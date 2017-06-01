// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <memory>
#include <set>
#include <string>

#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {
// First two are prefetchable, last one is not (see SetUp()).
const char kUrl[] = "http://www.google.com/cats";
const char kUrl2[] = "http://www.google.com/dogs";
const char kUrl3[] = "https://unknown.website/catsanddogs";
}

// Does nothing, controls which URLs are prefetchable.
class MockResourcePrefetcherPredictor : public ResourcePrefetchPredictor {
 public:
  MockResourcePrefetcherPredictor(const LoadingPredictorConfig& config,
                                  Profile* profile)
      : ResourcePrefetchPredictor(config, profile) {}

  bool IsUrlPrefetchable(const GURL& main_frame_url) const override {
    return prefetchable_urls_.find(main_frame_url) != prefetchable_urls_.end();
  }

  void AddPrefetchableUrl(const GURL& url) { prefetchable_urls_.insert(url); }

  MOCK_METHOD0(StartInitialization, void());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(StartPrefetching, void(const GURL&));
  MOCK_METHOD1(StopPrefeching, void(const GURL&));

 private:
  std::set<GURL> prefetchable_urls_;
};

class LoadingPredictorTest : public testing::Test {
 public:
  LoadingPredictorTest();
  ~LoadingPredictorTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<LoadingPredictor> predictor_;
  std::unique_ptr<TestingProfile> profile_;
};

LoadingPredictorTest::LoadingPredictorTest()
    : profile_(base::MakeUnique<TestingProfile>()) {}

LoadingPredictorTest::~LoadingPredictorTest() {
  profile_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

void LoadingPredictorTest::SetUp() {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  predictor_ = base::MakeUnique<LoadingPredictor>(config, profile_.get());
  auto mock =
      base::MakeUnique<MockResourcePrefetcherPredictor>(config, profile_.get());
  mock->AddPrefetchableUrl(GURL(kUrl));
  mock->AddPrefetchableUrl(GURL(kUrl2));
  predictor_->set_mock_resource_prefetch_predictor(std::move(mock));
  predictor_->StartInitialization();
  base::RunLoop().RunUntilIdle();
}

void LoadingPredictorTest::TearDown() {
  predictor_ = nullptr;
  profile_->DestroyHistoryService();
}

TEST_F(LoadingPredictorTest, TestPrefetchingDurationHistogram) {
  base::HistogramTester histogram_tester;

  const GURL url = GURL(kUrl);
  const GURL url2 = GURL(kUrl2);
  const GURL url3 = GURL(kUrl3);
  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  predictor_->CancelPageLoadHint(url);
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 1);

  // Mismatched start / end.
  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  predictor_->CancelPageLoadHint(url2);
  // No increment.
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 1);

  // Can track a navigation (url2) while one is still in progress (url).
  predictor_->PrepareForPageLoad(url2, HintOrigin::EXTERNAL);
  predictor_->CancelPageLoadHint(url2);
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 2);

  // Do not track non-prefetchable URLs.
  predictor_->PrepareForPageLoad(url3, HintOrigin::EXTERNAL);
  predictor_->CancelPageLoadHint(url3);
  // No increment.
  histogram_tester.ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 2);
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseCancelsHint) {
  const GURL url = GURL(kUrl);
  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  EXPECT_EQ(1UL, predictor_->active_hints_.size());

  auto summary = CreateURLRequestSummary(12, url.spec());
  predictor_->OnMainFrameResponse(summary);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameRequestCancelsStaleNavigations) {
  const std::string url = kUrl;
  const std::string url2 = kUrl2;
  const int tab_id = 12;
  const auto& active_navigations = predictor_->active_navigations_;
  const auto& active_hints = predictor_->active_hints_;

  auto summary = CreateURLRequestSummary(tab_id, url);
  auto navigation_id = CreateNavigationID(tab_id, url);

  predictor_->OnMainFrameRequest(summary);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_NE(active_hints.find(GURL(url)), active_hints.end());

  summary = CreateURLRequestSummary(tab_id, url2);
  predictor_->OnMainFrameRequest(summary);
  EXPECT_EQ(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_EQ(active_hints.find(GURL(url)), active_hints.end());

  auto navigation_id2 = CreateNavigationID(tab_id, url2);
  EXPECT_NE(active_navigations.find(navigation_id2), active_navigations.end());
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseClearsNavigations) {
  const std::string url = kUrl;
  const std::string redirected = kUrl2;
  const int tab_id = 12;
  const auto& active_navigations = predictor_->active_navigations_;
  const auto& active_hints = predictor_->active_hints_;

  auto summary = CreateURLRequestSummary(tab_id, url);
  auto navigation_id = CreateNavigationID(tab_id, url);

  predictor_->OnMainFrameRequest(summary);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());

  predictor_->OnMainFrameResponse(summary);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());

  // With redirects.
  predictor_->OnMainFrameRequest(summary);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());

  summary.redirect_url = GURL(redirected);
  predictor_->OnMainFrameRedirect(summary);
  EXPECT_FALSE(active_navigations.empty());
  EXPECT_FALSE(active_hints.empty());

  summary.navigation_id.main_frame_url = GURL(redirected);
  predictor_->OnMainFrameResponse(summary);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameRequestDoesntCancelExternalHint) {
  const GURL url = GURL(kUrl);
  const int tab_id = 12;
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // To check that the hint is not replaced, set the start time in the past,
  // and check later that it didn't change.
  base::TimeTicks start_time = it->second - base::TimeDelta::FromSeconds(10);
  it->second = start_time;

  auto summary = CreateURLRequestSummary(tab_id, url.spec());
  predictor_->OnMainFrameRequest(summary);
  EXPECT_NE(active_navigations.find(summary.navigation_id),
            active_navigations.end());
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_EQ(start_time, it->second);
}

TEST_F(LoadingPredictorTest, TestDontTrackNonPrefetchableUrls) {
  const GURL url3 = GURL(kUrl3);
  predictor_->PrepareForPageLoad(url3, HintOrigin::EXTERNAL);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

}  // namespace predictors
