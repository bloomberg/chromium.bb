// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/verbatim_match.h"

#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"

AutocompleteMatch VerbatimMatchForURL(
    AutocompleteProviderClient* client,
    const base::string16& input_text,
    metrics::OmniboxEventProto::PageClassification classification,
    int verbatim_relevance) {
  AutocompleteMatch match;
  client->Classify(input_text, false, true, classification, &match, nullptr);
  match.allowed_to_be_default_match = true;
  // The default relevance to use for relevance match. Should be greater than
  // all relevance matches returned by the ZeroSuggest server.
  const int kDefaultVerbatimRelevance = 1300;
  match.relevance =
      verbatim_relevance >= 0 ? verbatim_relevance : kDefaultVerbatimRelevance;
  return match;
}
