// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace predictors {

namespace {
const char kInitialUrl[] = "http://www.google.com/cats";
const char kRedirectedUrl[] = "http://www.google.com/dogs";
const char kRedirectedUrl2[] = "http://www.google.com/raccoons";
}

using Prediction = ResourcePrefetchPredictor::Prediction;
using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;
using PrefetchedRequestStats = ResourcePrefetcher::PrefetchedRequestStats;
using PrefetcherStats = ResourcePrefetcher::PrefetcherStats;

class LoadingStatsCollectorTest : public testing::Test {
 public:
  LoadingStatsCollectorTest();
  ~LoadingStatsCollectorTest() override;
  void SetUp() override;

  void TestRedirectStatusHistogram(const std::string& initial_url,
                                   const std::string& prediction_url,
                                   const std::string& navigation_url,
                                   RedirectStatus expected_status);

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<StrictMock<MockResourcePrefetchPredictor>> mock_predictor_;
  std::unique_ptr<LoadingStatsCollector> stats_collector_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

LoadingStatsCollectorTest::LoadingStatsCollectorTest()
    : profile_(base::MakeUnique<TestingProfile>()) {}

LoadingStatsCollectorTest::~LoadingStatsCollectorTest() = default;

void LoadingStatsCollectorTest::SetUp() {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  profile_ = base::MakeUnique<TestingProfile>();
  mock_predictor_ = base::MakeUnique<StrictMock<MockResourcePrefetchPredictor>>(
      config, profile_.get());
  stats_collector_ =
      base::MakeUnique<LoadingStatsCollector>(mock_predictor_.get(), config);
  histogram_tester_ = base::MakeUnique<base::HistogramTester>();
  content::RunAllBlockingPoolTasksUntilIdle();
}

void LoadingStatsCollectorTest::TestRedirectStatusHistogram(
    const std::string& initial_url,
    const std::string& prediction_url,
    const std::string& navigation_url,
    RedirectStatus expected_status) {
  // Prediction setting.
  // We need at least one resource for prediction.
  const std::string& script_url = "https://cdn.google.com/script.js";
  Prediction prediction = CreatePrediction(prediction_url, {GURL(script_url)});
  prediction.is_host = false;
  prediction.is_redirected = initial_url != prediction_url;
  EXPECT_CALL(*mock_predictor_, GetPrefetchData(GURL(initial_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Navigation simulation.
  URLRequestSummary script = CreateURLRequestSummary(
      1, navigation_url, script_url, content::RESOURCE_TYPE_SCRIPT);
  PageRequestSummary summary =
      CreatePageRequestSummary(navigation_url, initial_url, {script});

  stats_collector_->RecordPageRequestSummary(summary);

  // Histogram check.
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorRedirectStatusHistogram,
      static_cast<int>(expected_status), 1);
}

TEST_F(LoadingStatsCollectorTest, TestPrecisionRecallHistograms) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  const std::string script_url = "https://cdn.google.com/script.js";

  // Predicts 3 resources: 1 useful, 2 useless.
  Prediction prediction = CreatePrediction(
      main_frame_url,
      {GURL(script_url), GURL(script_url + "foo"), GURL(script_url + "bar")});
  EXPECT_CALL(*mock_predictor_, GetPrefetchData(GURL(main_frame_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Simulate a page load with 2 resources, one we know, one we don't.
  URLRequestSummary script = CreateURLRequestSummary(
      1, main_frame_url, script_url, content::RESOURCE_TYPE_SCRIPT);
  URLRequestSummary new_script = CreateURLRequestSummary(
      1, main_frame_url, script_url + "2", content::RESOURCE_TYPE_SCRIPT);
  PageRequestSummary summary = CreatePageRequestSummary(
      main_frame_url, main_frame_url, {script, new_script});

  stats_collector_->RecordPageRequestSummary(summary);

  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorRecallHistogram, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrecisionHistogram, 33, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorCountHistogram, 3, 1);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusNoRedirect) {
  TestRedirectStatusHistogram(kInitialUrl, kInitialUrl, kInitialUrl,
                              RedirectStatus::NO_REDIRECT);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusNoRedirectButPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kInitialUrl,
                              RedirectStatus::NO_REDIRECT_BUT_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusRedirectNotPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kInitialUrl, kRedirectedUrl,
                              RedirectStatus::REDIRECT_NOT_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusRedirectWrongPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kRedirectedUrl2,
                              RedirectStatus::REDIRECT_WRONG_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest,
       TestRedirectStatusRedirectCorrectlyPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kRedirectedUrl,
                              RedirectStatus::REDIRECT_CORRECTLY_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestPrefetchHitsMissesHistograms) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  const std::string& script_url = "https://cdn.google.com/script.js";
  EXPECT_CALL(*mock_predictor_, GetPrefetchData(GURL(main_frame_url), _))
      .WillOnce(Return(false));

  {
    // Initialize PrefetcherStats.
    PrefetchedRequestStats script1(GURL(script_url + "1"), false, 42 * 1024);
    PrefetchedRequestStats script2(GURL(script_url + "2"), true, 8 * 1024);
    PrefetchedRequestStats script3(GURL(script_url + "3"), false, 2 * 1024);
    PrefetchedRequestStats script4(GURL(script_url + "4"), true, 3 * 1024);
    auto stats = base::MakeUnique<PrefetcherStats>(GURL(main_frame_url));
    stats->requests_stats = {script1, script2, script3, script4};

    stats_collector_->RecordPrefetcherStats(std::move(stats));
  }

  {
    // Simulate a page load with 3 resources, two we prefetched, one we didn't.
    URLRequestSummary script1 = CreateURLRequestSummary(
        1, main_frame_url, script_url + "1", content::RESOURCE_TYPE_SCRIPT);
    URLRequestSummary script2 = CreateURLRequestSummary(
        1, main_frame_url, script_url + "2", content::RESOURCE_TYPE_SCRIPT);
    URLRequestSummary new_script = CreateURLRequestSummary(
        1, main_frame_url, script_url + "new", content::RESOURCE_TYPE_SCRIPT);
    PageRequestSummary summary = CreatePageRequestSummary(
        main_frame_url, main_frame_url, {script1, script2, new_script});

    stats_collector_->RecordPageRequestSummary(summary);
  }

  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchMissesCountCached, 1, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchMissesCountNotCached, 1, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchHitsCountCached, 1, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchHitsCountNotCached, 1, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchHitsSize, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kResourcePrefetchPredictorPrefetchMissesSize, 5, 1);
}

TEST_F(LoadingStatsCollectorTest, TestPreconnectHistograms) {
  const std::string main_frame_url("http://google.com/?query=cats");
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };
  EXPECT_CALL(*mock_predictor_, GetPrefetchData(GURL(main_frame_url), _))
      .WillOnce(Return(false));

  {
    // Initialize PreconnectStats.

    // These two are hits.
    PreconnectedRequestStats origin1(GURL(gen(1)).GetOrigin(), false, true);
    PreconnectedRequestStats origin2(GURL(gen(2)).GetOrigin(), true, false);
    // And these two are misses.
    PreconnectedRequestStats origin3(GURL(gen(3)).GetOrigin(), false, false);
    PreconnectedRequestStats origin4(GURL(gen(4)).GetOrigin(), true, true);

    auto stats = base::MakeUnique<PreconnectStats>(GURL(main_frame_url));
    stats->requests_stats = {origin1, origin2, origin3, origin4};

    stats_collector_->RecordPreconnectStats(std::move(stats));
  }

  {
    // Simulate a page load with 3 origins.
    URLRequestSummary script1 = CreateURLRequestSummary(
        1, main_frame_url, gen(1), content::RESOURCE_TYPE_SCRIPT);
    URLRequestSummary script2 = CreateURLRequestSummary(
        1, main_frame_url, gen(2), content::RESOURCE_TYPE_SCRIPT);
    URLRequestSummary script100 = CreateURLRequestSummary(
        1, main_frame_url, gen(100), content::RESOURCE_TYPE_SCRIPT);
    PageRequestSummary summary = CreatePageRequestSummary(
        main_frame_url, main_frame_url, {script1, script2, script100});

    stats_collector_->RecordPageRequestSummary(summary);
  }

  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveHitsPercentage, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectHitsPercentage, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveCount, 4, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectCount, 2, 1);
}

}  // namespace predictors
