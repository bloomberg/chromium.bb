// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace {

// TODO(hfung): The histogram code was copied and modified from
// search_provider.cc.  Refactor and consolidate the code.
// We keep track in a histogram how many suggest requests we send, how
// many suggest requests we invalidate (e.g., due to a user typing
// another character), and how many replies we receive.
// *** ADD NEW ENUMS AFTER ALL PREVIOUSLY DEFINED ONES! ***
//     (excluding the end-of-list enum value)
// We do not want values of existing enums to change or else it screws
// up the statistics.
enum ZeroSuggestRequestsHistogramValue {
  ZERO_SUGGEST_REQUEST_SENT = 1,
  ZERO_SUGGEST_REQUEST_INVALIDATED,
  ZERO_SUGGEST_REPLY_RECEIVED,
  ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxZeroSuggestRequest(
    ZeroSuggestRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.ZeroSuggestRequests", request_value,
                            ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE);
}

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

}  // namespace

// static
ZeroSuggestProvider* ZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new ZeroSuggestProvider(client, listener);
}

// static
void ZeroSuggestProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(omnibox::kZeroSuggestCachedResults,
                               std::string());
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  matches_.clear();
  if (!input.from_omnibox_focus() ||
      input.type() == metrics::OmniboxInputType::INVALID)
    return;

  Stop(true, false);
  set_field_trial_triggered(false);
  set_field_trial_triggered_in_session(false);
  results_from_cache_ = false;
  permanent_text_ = input.text();
  current_query_ = input.current_url().spec();
  current_page_classification_ = input.current_page_classification();
  current_url_match_ = MatchForCurrentURL();
  TemplateURLService* template_url_service = client()->GetTemplateURLService();

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (default_provider == NULL)
    return;

  base::string16 prefix;
  TemplateURLRef::SearchTermsArgs search_term_args(prefix);
  GURL suggest_url(default_provider->suggestions_url_ref().ReplaceSearchTerms(
      search_term_args, template_url_service->search_terms_data()));
  if (!suggest_url.is_valid())
    return;

  // No need to send the current page URL in personalized suggest or
  // most visited field trials.
  if (CanSendURL(input.current_url(), suggest_url, default_provider,
                 current_page_classification_,
                 template_url_service->search_terms_data(), client()) &&
      !OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial() &&
      !OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial()) {
    // Update suggest_url to include the current_page_url.
    search_term_args.current_page_url = current_query_;
    suggest_url =
        GURL(default_provider->suggestions_url_ref().ReplaceSearchTerms(
            search_term_args, template_url_service->search_terms_data()));
  } else if (!ShouldShowNonContextualZeroSuggest(suggest_url,
                                                 input.current_url())) {
    return;
  }

  done_ = false;
  // TODO(jered): Consider adding locally-sourced zero-suggestions here too.
  // These may be useful on the NTP or more relevant to the user than server
  // suggestions, if based on local browsing history.
  MaybeUseCachedSuggestions();
  Run(suggest_url);
}

void ZeroSuggestProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  if (fetcher_)
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_INVALIDATED);
  fetcher_.reset();
  waiting_for_most_visited_urls_request_ = false;
  done_ = true;

  if (clear_cached_results) {
    // We do not call Clear() on |results_| to retain |verbatim_relevance|
    // value in the |results_| object. |verbatim_relevance| is used at the
    // beginning of the next call to Start() to determine the current url
    // match relevance.
    results_.suggest_results.clear();
    results_.navigation_results.clear();
    current_query_.clear();
    most_visited_urls_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  if (OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial()) {
    // Remove the deleted match from the cache, so it is not shown to the user
    // again. Since we cannot remove just one result, blow away the cache.
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    std::string());
  }
  BaseSearchProvider::DeleteMatch(match);
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!results_.suggest_results.empty() ||
      !results_.navigation_results.empty() ||
      !most_visited_urls_.empty())
    provider_info->back().set_times_returned_results_in_session(1);
}

void ZeroSuggestProvider::ResetSession() {
  // The user has started editing in the omnibox, so leave
  // |field_trial_triggered_in_session| unchanged and set
  // |field_trial_triggered| to false since zero suggest is inactive now.
  set_field_trial_triggered(false);
}

ZeroSuggestProvider::ZeroSuggestProvider(AutocompleteProviderClient* client,
                                         AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client),
      listener_(listener),
      results_from_cache_(false),
      waiting_for_most_visited_urls_request_(false),
      weak_ptr_factory_(this) {
}

ZeroSuggestProvider::~ZeroSuggestProvider() {
}

const TemplateURL* ZeroSuggestProvider::GetTemplateURL(bool is_keyword) const {
  // Zero suggest provider should not receive keyword results.
  DCHECK(!is_keyword);
  return client()->GetTemplateURLService()->GetDefaultSearchProvider();
}

const AutocompleteInput ZeroSuggestProvider::GetInput(bool is_keyword) const {
  // The callers of this method won't look at the AutocompleteInput's
  // |from_omnibox_focus| member, so we can set its value to false.
  return AutocompleteInput(base::string16(), base::string16::npos,
                           std::string(), GURL(current_query_),
                           current_page_classification_, true, false, false,
                           true, false, client()->GetSchemeClassifier());
}

bool ZeroSuggestProvider::ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const {
  // We always use the default provider for search, so append the params.
  return true;
}

void ZeroSuggestProvider::RecordDeletionResult(bool success) {
  if (success) {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Success"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Failure"));
  }
}

void ZeroSuggestProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!done_);
  DCHECK_EQ(fetcher_.get(), source);

  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REPLY_RECEIVED);

  bool results_updated = false;
  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string json_data = SearchSuggestionParser::ExtractJsonData(source);
    scoped_ptr<base::Value> data(
        SearchSuggestionParser::DeserializeJsonData(json_data));
    if (data) {
      if (StoreSuggestionResponse(json_data, *data))
        return;
      results_updated = ParseSuggestResults(
          *data, kDefaultZeroSuggestRelevance, false, &results_);
    }
  }
  fetcher_.reset();
  done_ = true;
  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(results_updated);
}

bool ZeroSuggestProvider::StoreSuggestionResponse(
    const std::string& json_data,
    const base::Value& parsed_data) {
  if (!OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial() ||
      json_data.empty())
    return false;
  client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                  json_data);

  // If we received an empty result list, we should update the display, as it
  // may be showing cached results that should not be shown.
  const base::ListValue* root_list = NULL;
  const base::ListValue* results_list = NULL;
  if (parsed_data.GetAsList(&root_list) &&
      root_list->GetList(1, &results_list) &&
      results_list->empty())
    return false;

  // We are finished with the request and want to bail early.
  if (results_from_cache_)
    done_ = true;

  return results_from_cache_;
}

void ZeroSuggestProvider::AddSuggestResultsToMap(
    const SearchSuggestionParser::SuggestResults& results,
    MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i)
    AddMatchToMap(results[i], std::string(), i, false, false, map);
}

AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    const SearchSuggestionParser::NavigationResult& navigation) {
  AutocompleteMatch match(this, navigation.relevance(), false,
                          navigation.type());
  match.destination_url = navigation.url();

  // Zero suggest results should always omit protocols and never appear bold.
  const std::string languages(client()->GetAcceptLanguages());
  match.contents = url_formatter::FormatUrl(
      navigation.url(), languages, url_formatter::kFormatUrlOmitAll,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          navigation.url(), match.contents, client()->GetSchemeClassifier());

  AutocompleteMatch::ClassifyLocationInString(base::string16::npos, 0,
      match.contents.length(), ACMatchClassification::URL,
      &match.contents_class);

  match.description =
      AutocompleteMatch::SanitizeString(navigation.description());
  AutocompleteMatch::ClassifyLocationInString(base::string16::npos, 0,
      match.description.length(), ACMatchClassification::NONE,
      &match.description_class);
  return match;
}

void ZeroSuggestProvider::Run(const GURL& suggest_url) {
  if (OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial()) {
    most_visited_urls_.clear();
    scoped_refptr<history::TopSites> ts = client()->GetTopSites();
    if (ts) {
      waiting_for_most_visited_urls_request_ = true;
      ts->GetMostVisitedURLs(
          base::Bind(&ZeroSuggestProvider::OnMostVisitedUrlsAvailable,
                     weak_ptr_factory_.GetWeakPtr()), false);
    }
  } else {
    const int kFetcherID = 1;
    fetcher_ = net::URLFetcher::Create(kFetcherID, suggest_url,
                                       net::URLFetcher::GET, this);
    data_use_measurement::DataUseUserData::AttachToFetcher(
        fetcher_.get(), data_use_measurement::DataUseUserData::OMNIBOX);
    fetcher_->SetRequestContext(client()->GetRequestContext());
    fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
    // Add Chrome experiment state to the request headers.
    net::HttpRequestHeaders headers;
    variations::AppendVariationHeaders(fetcher_->GetOriginalURL(),
                                       client()->IsOffTheRecord(), false,
                                       &headers);
    fetcher_->SetExtraRequestHeaders(headers.ToString());
    fetcher_->Start();
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_SENT);
  }
}

void ZeroSuggestProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  if (!waiting_for_most_visited_urls_request_) return;
  most_visited_urls_ = urls;
  waiting_for_most_visited_urls_request_ = false;
  done_ = true;
  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(true);
}

void ZeroSuggestProvider::ConvertResultsToAutocompleteMatches() {
  matches_.clear();

  TemplateURLService* template_url_service = client()->GetTemplateURLService();
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // Fail if we can't set the clickthrough URL for query suggestions.
  if (default_provider == NULL ||
      !default_provider->SupportsReplacement(
          template_url_service->search_terms_data()))
    return;

  MatchMap map;
  AddSuggestResultsToMap(results_.suggest_results, &map);

  const int num_query_results = map.size();
  const int num_nav_results = results_.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.QueryResults", num_query_results);
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.URLResults", num_nav_results);
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.AllResults", num_results);

  // Show Most Visited results after ZeroSuggest response is received.
  if (OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial()) {
    if (!current_url_match_.destination_url.is_valid())
      return;
    matches_.push_back(current_url_match_);
    int relevance = 600;
    if (num_results > 0) {
      UMA_HISTOGRAM_COUNTS(
          "Omnibox.ZeroSuggest.MostVisitedResultsCounterfactual",
          most_visited_urls_.size());
    }
    const base::string16 current_query_string16(
        base::ASCIIToUTF16(current_query_));
    const std::string languages(client()->GetAcceptLanguages());
    for (size_t i = 0; i < most_visited_urls_.size(); i++) {
      const history::MostVisitedURL& url = most_visited_urls_[i];
      SearchSuggestionParser::NavigationResult nav(
          client()->GetSchemeClassifier(), url.url,
          AutocompleteMatchType::NAVSUGGEST, url.title, std::string(), false,
          relevance, true, current_query_string16, languages);
      matches_.push_back(NavigationToMatch(nav));
      --relevance;
    }
    return;
  }

  if (num_results == 0)
    return;

  // TODO(jered): Rip this out once the first match is decoupled from the
  // current typing in the omnibox.
  matches_.push_back(current_url_match_);

  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it)
    matches_.push_back(it->second);

  const SearchSuggestionParser::NavigationResults& nav_results(
      results_.navigation_results);
  for (SearchSuggestionParser::NavigationResults::const_iterator it(
           nav_results.begin()); it != nav_results.end(); ++it)
    matches_.push_back(NavigationToMatch(*it));
}

AutocompleteMatch ZeroSuggestProvider::MatchForCurrentURL() {
  // The placeholder suggestion for the current URL has high relevance so
  // that it is in the first suggestion slot and inline autocompleted. It
  // gets dropped as soon as the user types something.
  return VerbatimMatchForURL(client(), permanent_text_,
                             current_page_classification_,
                             results_.verbatim_relevance);
}

bool ZeroSuggestProvider::ShouldShowNonContextualZeroSuggest(
    const GURL& suggest_url,
    const GURL& current_page_url) const {
  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();
  if (!ZeroSuggestEnabled(suggest_url,
                          template_url_service->GetDefaultSearchProvider(),
                          current_page_classification_,
                          template_url_service->search_terms_data(), client()))
    return false;

  // If we cannot send URLs, then only the MostVisited and Personalized
  // variations can be shown.
  if (!OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial() &&
      !OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial())
    return false;

  // Only show zero suggest for HTTP[S] pages.
  // TODO(mariakhomenko): We may be able to expand this set to include pages
  // with other schemes (e.g. chrome://). That may require improvements to
  // the formatting of the verbatim result returned by MatchForCurrentURL().
  if (!current_page_url.is_valid() ||
      ((current_page_url.scheme() != url::kHttpScheme) &&
      (current_page_url.scheme() != url::kHttpsScheme)))
    return false;

  if (OmniboxFieldTrial::InZeroSuggestMostVisitedWithoutSerpFieldTrial() &&
      client()
          ->GetTemplateURLService()
          ->IsSearchResultsPageFromDefaultSearchProvider(current_page_url))
    return false;

  return true;
}

void ZeroSuggestProvider::MaybeUseCachedSuggestions() {
  if (!OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial())
    return;

  std::string json_data =
      client()->GetPrefs()->GetString(omnibox::kZeroSuggestCachedResults);
  if (!json_data.empty()) {
    scoped_ptr<base::Value> data(
        SearchSuggestionParser::DeserializeJsonData(json_data));
    if (data && ParseSuggestResults(
            *data, kDefaultZeroSuggestRelevance, false, &results_)) {
      ConvertResultsToAutocompleteMatches();
      results_from_cache_ = !matches_.empty();
    }
  }
}
