// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_classifier.h"

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "googleurl/src/gurl.h"

AutocompleteClassifier::AutocompleteClassifier(Profile* profile)
    : controller_(new AutocompleteController(profile)) {
}

AutocompleteClassifier::~AutocompleteClassifier() {
}

void AutocompleteClassifier::Classify(const string16& text,
                                      const string16& desired_tld,
                                      bool allow_exact_keyword_match,
                                      AutocompleteMatch* match,
                                      GURL* alternate_nav_url) {
  controller_->Start(text, desired_tld, true, false, allow_exact_keyword_match,
                     true);
  DCHECK(controller_->done());
  const AutocompleteResult& result = controller_->result();
  if (result.empty()) {
    if (alternate_nav_url)
      *alternate_nav_url = GURL();
    return;
  }

  DCHECK(result.default_match() != result.end());
  *match = *result.default_match();
  if (alternate_nav_url)
    *alternate_nav_url = result.alternate_nav_url();
}
