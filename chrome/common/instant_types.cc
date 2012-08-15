// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/instant_types.h"

InstantSuggestion::InstantSuggestion()
    : behavior(INSTANT_COMPLETE_NOW),
      type(INSTANT_SUGGESTION_SEARCH) {
}

InstantSuggestion::InstantSuggestion(const string16& in_text,
                                     InstantCompleteBehavior in_behavior,
                                     InstantSuggestionType in_type)
    : text(in_text),
      behavior(in_behavior),
      type(in_type) {
}

InstantSuggestion::~InstantSuggestion() {
}

InstantAutocompleteResult::InstantAutocompleteResult()
    : is_search(false),
      relevance(0) {
}

InstantAutocompleteResult::~InstantAutocompleteResult() {
}
