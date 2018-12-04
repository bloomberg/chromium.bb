// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FEATURES_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FEATURES_H_

#include "base/feature_list.h"

namespace contextual_suggestions {

extern const base::Feature kContextualSuggestionsButton;
extern const base::Feature kContextualSuggestionsIPHReverseScroll;
extern const base::Feature kContextualSuggestionsOptOut;

// Returns the minimum confidence threshold for showing contextual suggestions.
// The value will be in range [0.0, 1.0] (inclusive).
double GetMinimumConfidence();

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FEATURES_H_
