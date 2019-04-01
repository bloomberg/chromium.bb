// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

// Return an ACMatchClassifications structure given the |matches| to highlight.
// |matches| can be retrieved from calling FindTermMatches. |text_length| should
// be the full length (not the length of the truncated text clean returns) of
// the text being classified. It is used to ensure the the trailing
// classification is correct; i.e. if matches end at 20, and text_length is
// greater than 20, ClassifyTermMatches will add a non_match_style
// classification with offset 20. |match_style| and |non_match_style| specify
// the classifications to use for matched and non-matched text.
ACMatchClassifications ClassifyTermMatches(TermMatches matches,
                                           size_t text_length,
                                           int match_style,
                                           int non_match_style);

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_CLASSIFICATION_H_
