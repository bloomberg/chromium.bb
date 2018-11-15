// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_in_omnibox.h"

#include "base/feature_list.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url_service.h"

namespace {

bool SecurityLevelSafeForQueryInOmnibox(
    security_state::SecurityLevel security_level) {
  return security_level == security_state::SecurityLevel::SECURE ||
         security_level == security_state::SecurityLevel::EV_SECURE;
}

}  // namespace

QueryInOmnibox::QueryInOmnibox(AutocompleteClassifier* autocomplete_classifier,
                               TemplateURLService* template_url_service)
    : autocomplete_classifier_(autocomplete_classifier),
      template_url_service_(template_url_service) {
  DCHECK(autocomplete_classifier_);
  DCHECK(template_url_service_);
}

QueryInOmnibox::QueryInOmnibox()
    : autocomplete_classifier_(nullptr), template_url_service_(nullptr) {}

bool QueryInOmnibox::GetDisplaySearchTerms(
    security_state::SecurityLevel security_level,
    bool ignore_security_level,
    const GURL& url,
    base::string16* search_terms) {
  if (!base::FeatureList::IsEnabled(omnibox::kQueryInOmnibox))
    return false;

  if (!ignore_security_level &&
      !SecurityLevelSafeForQueryInOmnibox(security_level)) {
    return false;
  }

  base::string16 extracted_search_terms = ExtractSearchTermsInternal(url);
  if (extracted_search_terms.empty())
    return false;

  if (search_terms)
    *search_terms = extracted_search_terms;

  return true;
}

base::string16 QueryInOmnibox::ExtractSearchTermsInternal(const GURL& url) {
  if (url.is_empty())
    return base::string16();

  if (url != cached_url_) {
    cached_url_ = url;
    cached_search_terms_.clear();

    const TemplateURL* default_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (default_provider) {
      // If |url| doesn't match the default search provider,
      // |cached_search_terms_| will remain empty.
      default_provider->ExtractSearchTermsFromURL(
          url, template_url_service_->search_terms_data(),
          &cached_search_terms_);

      // Clear out the search terms if it looks like a URL.
      AutocompleteMatch match;
      autocomplete_classifier_->Classify(
          cached_search_terms_, false, false,
          metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
      if (!AutocompleteMatch::IsSearchType(match.type))
        cached_search_terms_.clear();
    }
  }

  return cached_search_terms_;
}
