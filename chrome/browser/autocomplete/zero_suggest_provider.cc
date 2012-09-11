// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_provider.h"

#include "base/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace {
// The relevance of the top match from this provider.
const int kMaxZeroSuggestRelevance = 100;
}  // namespace

ZeroSuggestProvider::ZeroSuggestProvider(
  AutocompleteProviderListener* listener,
  Profile* profile,
  const std::string& url_prefix)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_ZERO_SUGGEST),
      url_prefix_(url_prefix),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool /*minimal_changes*/) {
  UpdateMatches(input.text());
}

void ZeroSuggestProvider::StartZeroSuggest(const GURL& url,
                                           const string16& user_text) {
  DCHECK(url.is_valid());
  // Do not query non-http URLs. There will be no useful suggestions for https
  // or chrome URLs.
  if (url.scheme() != chrome::kHttpScheme)
    return;
  matches_.clear();
  done_ = false;
  user_text_ = user_text;
  current_query_ = url.spec();
  // TODO(jered): Consider adding locally-sourced zero-suggestions here too.
  // These may be useful on the NTP or more relevant to the user than server
  // suggestions, if based on local browsing history.
  Run();
}

void ZeroSuggestProvider::Stop(bool clear_cached_results) {
  fetcher_.reset();
  done_ = true;
  if (clear_cached_results) {
    results_.clear();
    current_query_.clear();
  }
}

void ZeroSuggestProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string json_data;
  source->GetResponseAsString(&json_data);
  const bool request_succeeded =
      source->GetStatus().is_success() && source->GetResponseCode() == 200;

  bool results_updated = false;
  if (request_succeeded) {
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> data(deserializer.Deserialize(NULL, NULL));
    results_updated = data.get() && ParseSuggestResults(data.get());
  }
  done_ = true;

  ConvertResultsToAutocompleteMatches();
  if (results_updated)
    listener_->OnProviderUpdate(true);
}

ZeroSuggestProvider::~ZeroSuggestProvider() {
}

void ZeroSuggestProvider::Run() {
  const int kFetcherID = 1;
  fetcher_.reset(
      net::URLFetcher::Create(kFetcherID,
          GURL(url_prefix_ + current_query_),
          net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(profile_->GetRequestContext());
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->Start();
}

void ZeroSuggestProvider::UpdateMatches(const string16& user_text) {
  user_text_ = user_text;
  const size_t prev_num_matches = matches_.size();
  ConvertResultsToAutocompleteMatches();
  if (matches_.size() != prev_num_matches)
    listener_->OnProviderUpdate(true);
}

bool ZeroSuggestProvider::ParseSuggestResults(Value* root_val) {
  std::string query;
  ListValue* root_list = NULL;
  ListValue* results = NULL;
  if (!root_val->GetAsList(&root_list) || !root_list->GetString(0, &query) ||
      (query != current_query_) || !root_list->GetList(1, &results))
    return false;

  results_.clear();
  ListValue* one_result = NULL;
  for (size_t index = 0; results->GetList(index, &one_result); ++index) {
    string16 result;
    one_result->GetString(0, &result);
    if (result.empty())
      continue;
    results_.push_back(result);
  }

  return true;
}

void ZeroSuggestProvider::ConvertResultsToAutocompleteMatches() {
  const TemplateURL* search_provider =
      template_url_service_->GetDefaultSearchProvider();
  // Fail if we can't set the clickthrough URL for query suggestions.
  if (search_provider == NULL || !search_provider->SupportsReplacement())
    return;
  matches_.clear();
  // Do not add anything if there are no results for this URL.
  if (results_.empty())
    return;
  AddMatchForCurrentURL();
  for (size_t i = 0; i < results_.size(); ++i)
    AddMatchForResult(search_provider, i, results_[i]);
}

// TODO(jered): Rip this out once the first match is decoupled from the current
// typing in the omnibox.
void ZeroSuggestProvider::AddMatchForCurrentURL() {
  // If the user has typed something besides the current url, they probably
  // don't intend to refresh it.
  const string16 current_query_text = ASCIIToUTF16(current_query_);
  const bool user_text_is_url = user_text_ == current_query_text;
  if (user_text_.empty() || user_text_is_url) {
    // The placeholder suggestion for the current URL has high relevance so
    // that it is in the first suggestion slot and inline autocompleted. It
    // gets dropped as soon as the user types something.
    AutocompleteMatch match(this, kMaxZeroSuggestRelevance, false,
                            AutocompleteMatch::NAVSUGGEST);
    match.destination_url = GURL(current_query_);
    match.contents = current_query_text;
    if (!user_text_is_url) {
      match.fill_into_edit = current_query_text;
      match.inline_autocomplete_offset = 0;
    }
    AutocompleteMatch::ClassifyLocationInString(0, current_query_.size(),
        match.contents.length(), ACMatchClassification::URL,
        &match.contents_class);
    matches_.push_back(match);
  }
}

void ZeroSuggestProvider::AddMatchForResult(
    const TemplateURL* search_provider,
    size_t result_index,
    const string16& result) {
  // TODO(jered): Rip out user_text_is_url logic when AddMatchForCurrentURL
  // goes away.
  const string16 current_query_text = ASCIIToUTF16(current_query_);
  const bool user_text_is_url = user_text_ == current_query_text;
  const bool kCaseInsensitve = false;
  if (!user_text_.empty() && !user_text_is_url &&
      !StartsWith(result, user_text_, kCaseInsensitve))
    // This suggestion isn't relevant for the current prefix.
    return;
  // This bogus relevance puts suggestions below the placeholder from
  // AddMatchForCurrentURL(), but very low so that after the user starts typing
  // zero-suggestions go away when there are other suggestions.
  // TODO(jered): Use real scores from the suggestion server.
  const int suggestion_relevance = kMaxZeroSuggestRelevance - matches_.size();
  AutocompleteMatch match(this, suggestion_relevance, false,
      AutocompleteMatch::SEARCH_SUGGEST);
  match.contents = result;
  match.fill_into_edit = result;
  if (!user_text_is_url && user_text_ != result)
    match.inline_autocomplete_offset = user_text_.length();

  // Build a URL for this query using the default search provider.
  const TemplateURLRef& search_url = search_provider->url_ref();
  DCHECK(search_url.SupportsReplacement());
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(result));
  match.search_terms_args->original_query = string16();
  match.search_terms_args->accepted_suggestion = result_index;
  match.destination_url =
      GURL(search_url.ReplaceSearchTerms(*match.search_terms_args.get()));

  if (user_text_.empty() || user_text_is_url || user_text_ == result) {
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  } else {
    // Style to look like normal search suggestions.
    match.contents_class.push_back(
       ACMatchClassification(0, ACMatchClassification::DIM));
    match.contents_class.push_back(
       ACMatchClassification(user_text_.length(), ACMatchClassification::NONE));
  }
  match.transition = content::PAGE_TRANSITION_GENERATED;

  matches_.push_back(match);
}
