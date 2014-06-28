// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_classifier.h"

#include "base/auto_reset.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "url/gurl.h"

// static
const int AutocompleteClassifier::kDefaultOmniboxProviders =
    AutocompleteProvider::TYPE_BOOKMARK |
    AutocompleteProvider::TYPE_BUILTIN |
    AutocompleteProvider::TYPE_HISTORY_QUICK |
    AutocompleteProvider::TYPE_HISTORY_URL |
    AutocompleteProvider::TYPE_KEYWORD |
    AutocompleteProvider::TYPE_SEARCH |
    AutocompleteProvider::TYPE_SHORTCUTS |
    AutocompleteProvider::TYPE_ZERO_SUGGEST;

AutocompleteClassifier::AutocompleteClassifier(
    scoped_ptr<AutocompleteController> controller,
    scoped_ptr<AutocompleteSchemeClassifier> scheme_classifier)
    : controller_(controller.Pass()),
      scheme_classifier_(scheme_classifier.Pass()),
      inside_classify_(false) {
}

AutocompleteClassifier::~AutocompleteClassifier() {
  // We should only reach here after Shutdown() has been called.
  DCHECK(!controller_.get());
}

void AutocompleteClassifier::Classify(
    const base::string16& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  DCHECK(!inside_classify_);
  base::AutoReset<bool> reset(&inside_classify_, true);
  controller_->Start(AutocompleteInput(
      text, base::string16::npos, base::string16(), GURL(),
      page_classification, true, prefer_keyword,
      allow_exact_keyword_match, false, *scheme_classifier_));
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

void AutocompleteClassifier::Shutdown() {
  controller_.reset();
}
