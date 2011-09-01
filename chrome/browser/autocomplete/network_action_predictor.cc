// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/network_action_predictor.h"

#include <math.h>

#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/profiles/profile.h"

namespace {

  const float kConfidenceCutoff[NetworkActionPredictor::LAST_PREDICT_ACTION] = {
    0.8f,
    0.5f
  };

}

NetworkActionPredictor::NetworkActionPredictor(Profile* profile)
  : profile_(profile) {
}

NetworkActionPredictor::~NetworkActionPredictor() {
}

// Given a match, return a recommended action.
NetworkActionPredictor::Action
    NetworkActionPredictor::RecommendAction(
        const string16& user_text, const AutocompleteMatch& match) const {
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return ACTION_NONE;

  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return ACTION_NONE;

  history::URLRow url_row;
  history::URLID url_id = url_db->GetRowForURL(match.destination_url, &url_row);

  if (url_id == 0)
    return ACTION_NONE;

  const double base_score = 1.0;

  // This constant is ln(1/0.8) so we end up decaying to 80% of the base score
  // for each week that passes.
  const double kLnDecayPercent = 0.2231435513142097;
  base::TimeDelta time_passed = base::Time::Now() - url_row.last_visit();

  // Clamp to 0.
  const double decay_exponent = std::max(0.0,
      kLnDecayPercent * static_cast<double>(time_passed.InMicroseconds()) /
          base::Time::kMicrosecondsPerWeek);

  const double kMaxDecaySpeedDivisor = 5.0;
  const double kNumUsesPerDecaySpeedDivisorIncrement = 1.0;
  const double decay_divisor = std::min(kMaxDecaySpeedDivisor,
      (url_row.typed_count() + kNumUsesPerDecaySpeedDivisorIncrement - 1) /
          kNumUsesPerDecaySpeedDivisorIncrement);

  const double confidence = base_score / exp(decay_exponent / decay_divisor);

  CHECK(confidence >= 0.0 && confidence <= 1.0);

  UMA_HISTOGRAM_COUNTS_100("NetworkActionPredictor.Confidence",
                           confidence * 100);

  for (int i = 0; i < LAST_PREDICT_ACTION; ++i)
    if (confidence >= kConfidenceCutoff[i])
      return static_cast<Action>(i);
  return ACTION_NONE;
}

// Return true if the suggestion type warrants a TCP/IP preconnection.
// i.e., it is now quite likely that the user will select the related domain.
// static
bool NetworkActionPredictor::IsPreconnectable(const AutocompleteMatch& match) {
  switch (match.type) {
    // Matches using the user's default search engine.
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
      // A match that uses a non-default search engine (e.g. for tab-to-search).
    case AutocompleteMatch::SEARCH_OTHER_ENGINE:
      return true;

    default:
      return false;
  }
}
