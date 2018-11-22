// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>

#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_ranker {
namespace {

// This tests that the TabRanker predictor returns a same score that is
// calcuated when the model is trained.
class TabScorePredictorTest : public testing::Test {
 public:
  TabScorePredictorTest() = default;
  ~TabScorePredictorTest() override = default;

 protected:
  // Returns a prediction for the tab example.
  float ScoreTab(const TabFeatures& tab) {
    float score = 0;
    EXPECT_EQ(TabRankerResult::kSuccess,
              tab_score_predictor_.ScoreTab(tab, &score));
    return score;
  }

 private:
  TabScorePredictor tab_score_predictor_;

  DISALLOW_COPY_AND_ASSIGN(TabScorePredictorTest);
};

}  // namespace

// Checks the score for an example that we have calculated a known score for
// outside of Chrome.
TEST_F(TabScorePredictorTest, KnownScore) {
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(-19.446667, ScoreTab(GetFullTabFeaturesForTesting()));
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeatures) {
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(5.7347188, ScoreTab(GetPartialTabFeaturesForTesting()));
}

}  // namespace tab_ranker
