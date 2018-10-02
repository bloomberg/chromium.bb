// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

namespace feed {
namespace metrics {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

// This needs to keep in sync with ActionType in third_party/feed/src/src/main/
// java/com/google/android/libraries/feed/host/logging/ActionType.java.
enum FeedActionType {
  UNKNOWN = -1,
  OPEN_URL = 1,
  OPEN_URL_INCOGNITO = 2,
  OPEN_URL_NEW_WINDOW = 3,
  OPEN_URL_NEW_TAB = 4,
  DOWNLOAD = 5,
};

TEST(FeedLoggingMetricsTest, ShouldLogOnSuggestionsShown) {
  base::HistogramTester histogram_tester;
  OnSuggestionShown(/*position=*/1, base::Time::Now(),
                    /*score=*/0.01f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  // Test corner cases for score.
  OnSuggestionShown(/*position=*/2, base::Time::Now(),
                    /*score=*/0.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*position=*/3, base::Time::Now(),
                    /*score=*/1.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*position=*/4, base::Time::Now(),
                    /*score=*/8.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.ContentSuggestions.Shown"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/2, /*count=*/1),
                  base::Bucket(/*min=*/3, /*count=*/1),
                  base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/10, /*count=*/1),
                  base::Bucket(/*min=*/11, /*count=*/1)));
}

TEST(FeedLoggingMetricsTest, ShouldLogOnPageShown) {
  base::HistogramTester histogram_tester;
  OnPageShown(/*content_count=*/10);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST(FeedLoggingMetricsTest, ShouldLogPrefetchedSuggestionsWhenOpened) {
  base::HistogramTester histogram_tester;
  OnSuggestionOpened(/*position=*/11, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::CURRENT_TAB);
  OnSuggestionOpened(/*position=*/13, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::CURRENT_TAB);
  OnSuggestionOpened(/*position=*/15, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::CURRENT_TAB);
  OnSuggestionOpened(/*position=*/23, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.ContentSuggestions.Opened"),
      ElementsAre(base::Bucket(/*min=*/11, /*count=*/1),
                  base::Bucket(/*min=*/13, /*count=*/1),
                  base::Bucket(/*min=*/15, /*count=*/1),
                  base::Bucket(/*min=*/23, /*count=*/1)));
}

}  // namespace
}  // namespace metrics
}  // namespace feed
