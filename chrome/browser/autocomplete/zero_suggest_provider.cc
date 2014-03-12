// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_provider.h"

#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
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

// The maximum relevance of the top match from this provider.
const int kDefaultVerbatimZeroSuggestRelevance = 1300;

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

}  // namespace

// static
ZeroSuggestProvider* ZeroSuggestProvider::Create(
    AutocompleteProviderListener* listener,
    Profile* profile) {
  return new ZeroSuggestProvider(listener, profile);
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool /*minimal_changes*/) {
}

void ZeroSuggestProvider::ResetSession() {
  // The user has started editing in the omnibox, so leave
  // |field_trial_triggered_in_session_| unchanged and set
  // |field_trial_triggered_| to false since zero suggest is inactive now.
  field_trial_triggered_ = false;
  Stop(true);
}

void ZeroSuggestProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  have_pending_request_ = false;
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REPLY_RECEIVED);

  std::string json_data;
  source->GetResponseAsString(&json_data);
  const bool request_succeeded =
      source->GetStatus().is_success() && source->GetResponseCode() == 200;

  if (request_succeeded) {
    scoped_ptr<base::Value> data(DeserializeJsonData(json_data));
    if (data.get())
      ParseSuggestResults(*data.get(), false, &results_);
  }
  done_ = true;

  ConvertResultsToAutocompleteMatches();
  if (!matches_.empty())
    listener_->OnProviderUpdate(true);
}

void ZeroSuggestProvider::StartZeroSuggest(
    const GURL& current_page_url,
    AutocompleteInput::PageClassification page_classification,
    const base::string16& permanent_text) {
  Stop(true);
  field_trial_triggered_ = false;
  field_trial_triggered_in_session_ = false;
  permanent_text_ = permanent_text;
  current_query_ = current_page_url.spec();
  current_page_classification_ = page_classification;
  current_url_match_ = MatchForCurrentURL();

  const TemplateURL* default_provider =
     template_url_service_->GetDefaultSearchProvider();
  if (default_provider == NULL)
    return;
  base::string16 prefix;
  TemplateURLRef::SearchTermsArgs search_term_args(prefix);
  search_term_args.current_page_url = current_query_;
  GURL suggest_url(default_provider->suggestions_url_ref().
                   ReplaceSearchTerms(search_term_args));
  if (!CanSendURL(current_page_url, suggest_url,
          template_url_service_->GetDefaultSearchProvider(),
          page_classification, profile_) ||
      !OmniboxFieldTrial::InZeroSuggestFieldTrial())
    return;
  done_ = false;
  // TODO(jered): Consider adding locally-sourced zero-suggestions here too.
  // These may be useful on the NTP or more relevant to the user than server
  // suggestions, if based on local browsing history.
  Run(suggest_url);
}

ZeroSuggestProvider::ZeroSuggestProvider(
  AutocompleteProviderListener* listener,
  Profile* profile)
    : BaseSearchProvider(listener, profile,
                         AutocompleteProvider::TYPE_ZERO_SUGGEST),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)),
      have_pending_request_(false),
      weak_ptr_factory_(this) {
}

ZeroSuggestProvider::~ZeroSuggestProvider() {
}

const TemplateURL* ZeroSuggestProvider::GetTemplateURL(
    const SuggestResult& result) const {
  // Zero suggest provider should not receive keyword results.
  DCHECK(!result.from_keyword_provider());
  return template_url_service_->GetDefaultSearchProvider();
}

const AutocompleteInput ZeroSuggestProvider::GetInput(bool is_keyword) const {
  return AutocompleteInput(
      base::string16(), base::string16::npos, base::string16(),
      GURL(current_query_), current_page_classification_, true, false, false,
      AutocompleteInput::ALL_MATCHES);
}

bool ZeroSuggestProvider::ShouldAppendExtraParams(
      const SuggestResult& result) const {
  // We always use the default provider for search, so append the params.
  return true;
}

void ZeroSuggestProvider::StopSuggest() {
  if (have_pending_request_)
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_INVALIDATED);
  have_pending_request_ = false;
  fetcher_.reset();
}

void ZeroSuggestProvider::ClearAllResults() {
  // We do not call Clear() on |results_| to retain |verbatim_relevance|
  // value in the |results_| object. |verbatim_relevance| is used at the
  // beginning of the next StartZeroSuggest() call to determine the current url
  // match relevance.
  results_.suggest_results.clear();
  results_.navigation_results.clear();
  current_query_.clear();
  matches_.clear();
}

int ZeroSuggestProvider::GetDefaultResultRelevance() const {
  return kDefaultZeroSuggestRelevance;
}

void ZeroSuggestProvider::RecordDeletionResult(bool success) {
  if (success) {
    content::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Success"));
  } else {
    content::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Failure"));
  }
}

void ZeroSuggestProvider::AddSuggestResultsToMap(
    const SuggestResults& results,
    MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i)
    AddMatchToMap(results[i], std::string(), i, map);
}

AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    const NavigationResult& navigation) {
  AutocompleteMatch match(this, navigation.relevance(), false,
                          AutocompleteMatchType::NAVSUGGEST);
  match.destination_url = navigation.url();

  // Zero suggest results should always omit protocols and never appear bold.
  const std::string languages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  match.contents = net::FormatUrl(navigation.url(), languages,
      net::kFormatUrlOmitAll, net::UnescapeRule::SPACES, NULL, NULL, NULL);
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(navigation.url(),
          match.contents);

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
  have_pending_request_ = false;
  const int kFetcherID = 1;
  fetcher_.reset(
      net::URLFetcher::Create(kFetcherID,
          suggest_url,
          net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(profile_->GetRequestContext());
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  chrome_variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      fetcher_->GetOriginalURL(), profile_->IsOffTheRecord(), false, &headers);
  fetcher_->SetExtraRequestHeaders(headers.ToString());

  fetcher_->Start();

  if (OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial()) {
    most_visited_urls_.clear();
    history::TopSites* ts = profile_->GetTopSites();
    if (ts) {
      ts->GetMostVisitedURLs(
          base::Bind(&ZeroSuggestProvider::OnMostVisitedUrlsAvailable,
                     weak_ptr_factory_.GetWeakPtr()), false);
    }
  }
  have_pending_request_ = true;
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_SENT);
}

void ZeroSuggestProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  most_visited_urls_ = urls;
}

void ZeroSuggestProvider::ConvertResultsToAutocompleteMatches() {
  matches_.clear();

  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  // Fail if we can't set the clickthrough URL for query suggestions.
  if (default_provider == NULL || !default_provider->SupportsReplacement())
    return;

  MatchMap map;
  AddSuggestResultsToMap(results_.suggest_results, &map);

  const int num_query_results = map.size();
  const int num_nav_results = results_.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.QueryResults", num_query_results);
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.URLResults",  num_nav_results);
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
    const std::string languages(
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
    for (size_t i = 0; i < most_visited_urls_.size(); i++) {
      const history::MostVisitedURL& url = most_visited_urls_[i];
      NavigationResult nav(*this, url.url, url.title, false, relevance, true,
          current_query_string16, languages);
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

  const NavigationResults& nav_results(results_.navigation_results);
  for (NavigationResults::const_iterator it(nav_results.begin());
       it != nav_results.end(); ++it)
    matches_.push_back(NavigationToMatch(*it));
}

AutocompleteMatch ZeroSuggestProvider::MatchForCurrentURL() {
  AutocompleteInput input(permanent_text_, base::string16::npos, base::string16(),
                          GURL(current_query_), current_page_classification_,
                          false, false, true, AutocompleteInput::ALL_MATCHES);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
      permanent_text_, false, true, current_page_classification_, &match, NULL);
  match.is_history_what_you_typed_match = false;
  match.allowed_to_be_default_match = true;

  // The placeholder suggestion for the current URL has high relevance so
  // that it is in the first suggestion slot and inline autocompleted. It
  // gets dropped as soon as the user types something.
  match.relevance = GetVerbatimRelevance();

  return match;
}

int ZeroSuggestProvider::GetVerbatimRelevance() const {
  return results_.verbatim_relevance >= 0 ?
      results_.verbatim_relevance : kDefaultVerbatimZeroSuggestRelevance;
}
