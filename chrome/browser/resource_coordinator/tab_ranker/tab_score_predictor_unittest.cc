// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>

#include "chrome/browser/resource_coordinator/tab_ranker/mru_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/window_features.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace tab_ranker {
namespace {

// Returns a fairly typical set of window features.
WindowFeatures GetWindowFeatures() {
  WindowFeatures window(SessionID::NewUnique(),
                        metrics::WindowMetricsEvent::TYPE_TABBED);
  window.tab_count = 3;
  window.is_active = true;
  window.show_state = metrics::WindowMetricsEvent::SHOW_STATE_NORMAL;
  return window;
}

// These tests verify that the example_preprocessor_config.pb and
// native_inference.h code together generates correct scores.
class TabScorePredictorTest : public testing::Test {
 public:
  TabScorePredictorTest() = default;
  ~TabScorePredictorTest() override = default;

 protected:
  // Returns a prediction for the tab example.
  float ScoreTab(const TabFeatures& tab,
                 const WindowFeatures& window,
                 const MRUFeatures& mru) {
    float score = 0;
    EXPECT_EQ(TabRankerResult::kSuccess,
              tab_score_predictor_.ScoreTab(tab, window, mru, &score));
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
  MRUFeatures mru;
  mru.index = 27;
  mru.total = 30;

  TabFeatures tab;
  tab.has_before_unload_handler = true;
  tab.has_form_entry = true;
  tab.host = "www.google.com";
  tab.is_pinned = true;
  tab.key_event_count = 21;
  tab.mouse_event_count = 22;
  tab.navigation_entry_count = 24;
  tab.num_reactivations = 25;
  tab.page_transition_core_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  tab.page_transition_from_address_bar = true;
  tab.page_transition_is_redirect = true;
  tab.site_engagement_score = 26;
  tab.time_from_backgrounded = 10000;
  tab.touch_event_count = 28;
  tab.was_recently_audible = true;

  WindowFeatures window(GetWindowFeatures());
  window.tab_count = 27;

  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(1.8338085, ScoreTab(tab, window, mru));
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeatures) {
  MRUFeatures mru;
  mru.index = 13;
  mru.total = 130;

  TabFeatures tab;
  tab.has_before_unload_handler = true;
  tab.has_form_entry = true;
  tab.host = "www.example.com";
  tab.is_pinned = true;
  tab.key_event_count = 121;
  tab.mouse_event_count = 122;
  tab.navigation_entry_count = 124;
  tab.num_reactivations = 125;
  tab.page_transition_from_address_bar = true;
  tab.page_transition_is_redirect = true;
  tab.time_from_backgrounded = 110000;
  tab.touch_event_count = 128;
  tab.was_recently_audible = true;

  WindowFeatures window(GetWindowFeatures());
  window.tab_count = 127;

  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(8.7163248, ScoreTab(tab, window, mru));
}

}  // namespace tab_ranker
