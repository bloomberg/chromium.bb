// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_match_classification.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/scored_history_match.h"
#include "in_memory_url_index_types.h"

ACMatchClassifications ClassifyTermMatches(TermMatches matches,
                                           size_t text_length,
                                           int match_style,
                                           int non_match_style) {
  ACMatchClassifications classes;
  if (matches.empty()) {
    if (text_length)
      classes.push_back(ACMatchClassification(0, non_match_style));
    return classes;
  }
  if (matches[0].offset)
    classes.push_back(ACMatchClassification(0, non_match_style));
  size_t match_count = matches.size();
  for (size_t i = 0; i < match_count;) {
    size_t offset = matches[i].offset;
    classes.push_back(ACMatchClassification(offset, match_style));
    // Skip all adjacent matches.
    do {
      offset += matches[i].length;
      ++i;
    } while ((i < match_count) && (offset == matches[i].offset));
    if (offset < text_length)
      classes.push_back(ACMatchClassification(offset, non_match_style));
  }
  return classes;
}
