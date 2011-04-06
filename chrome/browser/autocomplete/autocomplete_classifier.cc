// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_classifier.h"

#include "base/auto_reset.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "googleurl/src/gurl.h"

AutocompleteClassifier::AutocompleteClassifier(Profile* profile)
    : controller_(new AutocompleteController(profile, NULL)),
      inside_classify_(false) {
}

AutocompleteClassifier::~AutocompleteClassifier() {
}

void AutocompleteClassifier::Classify(const string16& text,
                                      const string16& desired_tld,
                                      bool allow_exact_keyword_match,
                                      AutocompleteMatch* match,
                                      GURL* alternate_nav_url) {
  DCHECK(!inside_classify_);
  AutoReset<bool> reset(&inside_classify_, true);
  controller_->Start(text, desired_tld, true, false, allow_exact_keyword_match,
                     AutocompleteInput::BEST_MATCH);
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
