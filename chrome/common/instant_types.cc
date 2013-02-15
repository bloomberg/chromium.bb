// Copyright 2012 The Chromium Authors. All rights reserved.
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
    : transition(content::PAGE_TRANSITION_LINK),
      relevance(0) {
}

InstantAutocompleteResult::~InstantAutocompleteResult() {
}

ThemeBackgroundInfo::ThemeBackgroundInfo()
    : color_r(0),
      color_g(0),
      color_b(0),
      color_a(0),
      image_horizontal_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_vertical_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_top_offset(0),
      image_tiling(THEME_BKGRND_IMAGE_NO_REPEAT),
      image_height(0) {
}

ThemeBackgroundInfo::~ThemeBackgroundInfo() {
}
