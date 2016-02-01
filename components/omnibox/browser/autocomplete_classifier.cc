// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_classifier.h"

#include <utility>

#include "base/auto_reset.h"
#include "build/build_config.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "url/gurl.h"

// static
const int AutocompleteClassifier::kDefaultOmniboxProviders =
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    // Custom search engines cannot be used on mobile..
    AutocompleteProvider::TYPE_KEYWORD |
#endif
#if !defined(OS_IOS)
    // "Builtin", "Shortcuts" and "Zero Suggest" are not supported on iOS.
    AutocompleteProvider::TYPE_BUILTIN |
    AutocompleteProvider::TYPE_SHORTCUTS |
    AutocompleteProvider::TYPE_ZERO_SUGGEST |
#else
    // "URL from clipboard" can only be used on iOS.
    AutocompleteProvider::TYPE_CLIPBOARD_URL |
#endif
    AutocompleteProvider::TYPE_BOOKMARK |
    AutocompleteProvider::TYPE_HISTORY_QUICK |
    AutocompleteProvider::TYPE_HISTORY_URL |
    AutocompleteProvider::TYPE_SEARCH;

AutocompleteClassifier::AutocompleteClassifier(
    scoped_ptr<AutocompleteController> controller,
    scoped_ptr<AutocompleteSchemeClassifier> scheme_classifier)
    : controller_(std::move(controller)),
      scheme_classifier_(std::move(scheme_classifier)),
      inside_classify_(false) {}

AutocompleteClassifier::~AutocompleteClassifier() {
  // We should only reach here after Shutdown() has been called.
  DCHECK(!controller_.get());
}

void AutocompleteClassifier::Shutdown() {
  controller_.reset();
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
      text, base::string16::npos, std::string(), GURL(), page_classification,
      true, prefer_keyword, allow_exact_keyword_match, false, false,
      *scheme_classifier_));
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
