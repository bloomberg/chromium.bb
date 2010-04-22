// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_versus_navigate_classifier.h"

#include "chrome/browser/autocomplete/autocomplete.h"
#include "googleurl/src/gurl.h"

SearchVersusNavigateClassifier::SearchVersusNavigateClassifier(Profile* profile)
    : controller_(new AutocompleteController(profile)) {
}

SearchVersusNavigateClassifier::~SearchVersusNavigateClassifier() {
}

void SearchVersusNavigateClassifier::Classify(const std::wstring& text,
    const std::wstring& desired_tld,
    bool* is_search,
    GURL* destination_url,
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    GURL* alternate_nav_url) {
  controller_->Start(text, desired_tld, true, false, true);
  DCHECK(controller_->done());
  const AutocompleteResult& result = controller_->result();
  if (result.empty()) {
    if (is_search)
      *is_search = false;
    if (destination_url)
      *destination_url = GURL();
    if (transition)
      *transition = PageTransition::TYPED;
    if (is_history_what_you_typed_match)
      *is_history_what_you_typed_match = false;
    if (alternate_nav_url)
      *alternate_nav_url = GURL();
    return;
  }

  const AutocompleteResult::const_iterator match(result.default_match());
  DCHECK(match != result.end());

  // If this is a search, the page transition will be GENERATED rather than
  // TYPED.
  if (is_search)
    *is_search = (match->transition != PageTransition::TYPED);
  if (destination_url)
    *destination_url = match->destination_url;
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url)
    *alternate_nav_url = result.alternate_nav_url();
}
