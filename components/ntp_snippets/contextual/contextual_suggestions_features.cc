// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_suggestions {

const base::Feature kContextualSuggestionsButton{
    "ContextualSuggestionsButton", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSuggestionsIPHReverseScroll{
    "ContextualSuggestionsIPHReverseScroll", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextualSuggestionsOptOut{
    "ContextualSuggestionsOptOut", base::FEATURE_DISABLED_BY_DEFAULT};

// Default minimum confidence value for contextual suggestions.
static constexpr double kMinimumConfidenceDefault = .75;

// Provides ability to customize the minimum confidence parameter.
base::FeatureParam<double> kContextualSuggestionsMinimumConfidenceParam{
    &kContextualSuggestionsButton, "minimum_confidence",
    kMinimumConfidenceDefault};

double GetMinimumConfidence() {
  double minimum_confidence =
      kContextualSuggestionsMinimumConfidenceParam.Get();
  LOG_IF(ERROR, minimum_confidence < 0.0 || minimum_confidence > 1.0)
      << "Specified minimum confidence outside of allowed range.";
  return std::max(0.0, std::min(minimum_confidence, 1.0));
}

}  // namespace contextual_suggestions
