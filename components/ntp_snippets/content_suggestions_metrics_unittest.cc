// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_metrics.h"

#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace metrics {
namespace {

using testing::ElementsAre;

TEST(ContentSuggestionsMetricsTest, ShouldLogOnSuggestionsShown) {
  base::HistogramTester histogram_tester;
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*category_position=*/3, base::Time::Now(), 0.01f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  // Test corner cases for score.
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*category_position=*/3, base::Time::Now(), 0.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*category_position=*/3, base::Time::Now(), 1.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*category_position=*/3, base::Time::Now(), 8.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/10, /*count=*/1),
                  base::Bucket(/*min=*/11, /*count=*/1)));
}

}  // namespace
}  // namespace metrics
}  // namespace ntp_snippets
