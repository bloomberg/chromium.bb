// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/base_search_provider.h"

#include "base/strings/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

// BaseSearchProvider ---------------------------------------------------------

BaseSearchProvider::BaseSearchProvider(AutocompleteProviderListener* listener,
                                       Profile* profile,
                                       AutocompleteProvider::Type type)
    : AutocompleteProvider(listener, profile, type) {}

BaseSearchProvider::~BaseSearchProvider() {}

// BaseSearchProvider::Result --------------------------------------------------

BaseSearchProvider::Result::Result(bool from_keyword_provider,
                                   int relevance,
                                   bool relevance_from_server)
    : from_keyword_provider_(from_keyword_provider),
      relevance_(relevance),
      relevance_from_server_(relevance_from_server) {}

BaseSearchProvider::Result::~Result() {}

// BaseSearchProvider::SuggestResult -------------------------------------------

BaseSearchProvider::SuggestResult::SuggestResult(
    const base::string16& suggestion,
    AutocompleteMatchType::Type type,
    const base::string16& match_contents,
    const base::string16& annotation,
    const std::string& suggest_query_params,
    const std::string& deletion_url,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    bool should_prefetch,
    const base::string16& input_text)
    : Result(from_keyword_provider, relevance, relevance_from_server),
      suggestion_(suggestion),
      type_(type),
      annotation_(annotation),
      suggest_query_params_(suggest_query_params),
      deletion_url_(deletion_url),
      should_prefetch_(should_prefetch) {
  match_contents_ = match_contents;
  DCHECK(!match_contents_.empty());
  ClassifyMatchContents(true, input_text);
}

BaseSearchProvider::SuggestResult::~SuggestResult() {}

void BaseSearchProvider::SuggestResult::ClassifyMatchContents(
    const bool allow_bolding_all,
    const base::string16& input_text) {
  size_t input_position = match_contents_.find(input_text);
  if (!allow_bolding_all && (input_position == base::string16::npos)) {
    // Bail if the code below to update the bolding would bold the whole
    // string.  Note that the string may already be entirely bolded; if
    // so, leave it as is.
    return;
  }
  match_contents_class_.clear();
  // We do intra-string highlighting for suggestions - the suggested segment
  // will be highlighted, e.g. for input_text = "you" the suggestion may be
  // "youtube", so we'll bold the "tube" section: you*tube*.
  if (input_text != match_contents_) {
    if (input_position == base::string16::npos) {
      // The input text is not a substring of the query string, e.g. input
      // text is "slasdot" and the query string is "slashdot", so we bold the
      // whole thing.
      match_contents_class_.push_back(
          ACMatchClassification(0, ACMatchClassification::MATCH));
    } else {
      // We don't iterate over the string here annotating all matches because
      // it looks odd to have every occurrence of a substring that may be as
      // short as a single character highlighted in a query suggestion result,
      // e.g. for input text "s" and query string "southwest airlines", it
      // looks odd if both the first and last s are highlighted.
      if (input_position != 0) {
        match_contents_class_.push_back(
            ACMatchClassification(0, ACMatchClassification::MATCH));
      }
      match_contents_class_.push_back(
          ACMatchClassification(input_position, ACMatchClassification::NONE));
      size_t next_fragment_position = input_position + input_text.length();
      if (next_fragment_position < match_contents_.length()) {
        match_contents_class_.push_back(ACMatchClassification(
            next_fragment_position, ACMatchClassification::MATCH));
      }
    }
  } else {
    // Otherwise, match_contents_ is a verbatim (what-you-typed) match, either
    // for the default provider or a keyword search provider.
    match_contents_class_.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }
}

bool BaseSearchProvider::SuggestResult::IsInlineable(
    const base::string16& input) const {
  return StartsWith(suggestion_, input, false);
}

int BaseSearchProvider::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_provider_ && keyword_provider_requested)
    return 100;
  return ((input.type() == AutocompleteInput::URL) ? 300 : 600);
}

// BaseSearchProvider::NavigationResult ----------------------------------------

BaseSearchProvider::NavigationResult::NavigationResult(
    const AutocompleteProvider& provider,
    const GURL& url,
    const base::string16& description,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    const base::string16& input_text,
    const std::string& languages)
    : Result(from_keyword_provider, relevance, relevance_from_server),
      url_(url),
      formatted_url_(AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          provider.StringForURLDisplay(url, true, false))),
      description_(description) {
  DCHECK(url_.is_valid());
  CalculateAndClassifyMatchContents(true, input_text, languages);
}

BaseSearchProvider::NavigationResult::~NavigationResult() {}

void BaseSearchProvider::NavigationResult::CalculateAndClassifyMatchContents(
    const bool allow_bolding_nothing,
    const base::string16& input_text,
    const std::string& languages) {
  // First look for the user's input inside the formatted url as it would be
  // without trimming the scheme, so we can find matches at the beginning of the
  // scheme.
  const URLPrefix* prefix =
      URLPrefix::BestURLPrefix(formatted_url_, input_text);
  size_t match_start = (prefix == NULL) ? formatted_url_.find(input_text)
                                        : prefix->prefix.length();
  bool trim_http = !AutocompleteInput::HasHTTPScheme(input_text) &&
                   (!prefix || (match_start != 0));
  const net::FormatUrlTypes format_types =
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP);

  base::string16 match_contents = net::FormatUrl(url_, languages, format_types,
      net::UnescapeRule::SPACES, NULL, NULL, &match_start);
  // If the first match in the untrimmed string was inside a scheme that we
  // trimmed, look for a subsequent match.
  if (match_start == base::string16::npos)
    match_start = match_contents.find(input_text);
  // Update |match_contents_| and |match_contents_class_| if it's allowed.
  if (allow_bolding_nothing || (match_start != base::string16::npos)) {
    match_contents_ = match_contents;
    // Safe if |match_start| is npos; also safe if the input is longer than the
    // remaining contents after |match_start|.
    AutocompleteMatch::ClassifyLocationInString(match_start,
        input_text.length(), match_contents_.length(),
        ACMatchClassification::URL, &match_contents_class_);
  }
}

bool BaseSearchProvider::NavigationResult::IsInlineable(
    const base::string16& input) const {
  return URLPrefix::BestURLPrefix(formatted_url_, input) != NULL;
}

int BaseSearchProvider::NavigationResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  return (from_keyword_provider_ || !keyword_provider_requested) ? 800 : 150;
}

// BaseSearchProvider::Results -------------------------------------------------

BaseSearchProvider::Results::Results() : verbatim_relevance(-1) {}

BaseSearchProvider::Results::~Results() {}

void BaseSearchProvider::Results::Clear() {
  suggest_results.clear();
  navigation_results.clear();
  verbatim_relevance = -1;
  metadata.clear();
}

bool BaseSearchProvider::Results::HasServerProvidedScores() const {
  if (verbatim_relevance >= 0)
    return true;

  // Right now either all results of one type will be server-scored or they will
  // all be locally scored, but in case we change this later, we'll just check
  // them all.
  for (SuggestResults::const_iterator i(suggest_results.begin());
       i != suggest_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }
  for (NavigationResults::const_iterator i(navigation_results.begin());
       i != navigation_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }

  return false;
}

