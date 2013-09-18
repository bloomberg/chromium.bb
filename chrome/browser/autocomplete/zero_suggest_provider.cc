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
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
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

void ZeroSuggestProvider::Stop(bool clear_cached_results) {
  if (have_pending_request_)
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_INVALIDATED);
  have_pending_request_ = false;
  fetcher_.reset();
  done_ = true;
  if (clear_cached_results) {
    query_matches_map_.clear();
    navigation_results_.clear();
    current_query_.clear();
    matches_.clear();
  }
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  std::vector<uint32> field_trial_hashes;
  OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
  for (size_t i = 0; i < field_trial_hashes.size(); ++i) {
    if (field_trial_triggered_)
      new_entry.mutable_field_trial_triggered()->Add(field_trial_hashes[i]);
    if (field_trial_triggered_in_session_) {
      new_entry.mutable_field_trial_triggered_in_session()->Add(
          field_trial_hashes[i]);
     }
  }
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
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> data(deserializer.Deserialize(NULL, NULL));
    if (data.get())
      ParseSuggestResults(*data.get());
  }
  done_ = true;

  ConvertResultsToAutocompleteMatches();
  if (!matches_.empty())
    listener_->OnProviderUpdate(true);
}

void ZeroSuggestProvider::StartZeroSuggest(
    const GURL& url,
    AutocompleteInput::PageClassification page_classification,
    const string16& permanent_text) {
  Stop(true);
  field_trial_triggered_ = false;
  field_trial_triggered_in_session_ = false;
  if (!ShouldRunZeroSuggest(url))
    return;
  verbatim_relevance_ = kDefaultVerbatimZeroSuggestRelevance;
  done_ = false;
  permanent_text_ = permanent_text;
  current_query_ = url.spec();
  current_page_classification_ = page_classification;
  current_url_match_ = MatchForCurrentURL();
  // TODO(jered): Consider adding locally-sourced zero-suggestions here too.
  // These may be useful on the NTP or more relevant to the user than server
  // suggestions, if based on local browsing history.
  Run();
}

ZeroSuggestProvider::ZeroSuggestProvider(
  AutocompleteProviderListener* listener,
  Profile* profile)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_ZERO_SUGGEST),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)),
      have_pending_request_(false),
      verbatim_relevance_(kDefaultVerbatimZeroSuggestRelevance),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false),
      weak_ptr_factory_(this) {
}

ZeroSuggestProvider::~ZeroSuggestProvider() {
}

bool ZeroSuggestProvider::ShouldRunZeroSuggest(const GURL& url) const {
  if (!ShouldSendURL(url))
    return false;

  // Don't run if there's no profile or in incognito mode.
  if (profile_ == NULL || profile_->IsOffTheRecord())
    return false;

  // Don't run if we can't get preferences or search suggest is not enabled.
  PrefService* prefs = profile_->GetPrefs();
  if (prefs == NULL || !prefs->GetBoolean(prefs::kSearchSuggestEnabled))
    return false;

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  browser_sync::SyncPrefs sync_prefs(prefs);
  // The user has needs to have Chrome Sync enabled (for permissions to
  // transmit their current URL) and be in the field trial.
  if (!OmniboxFieldTrial::InZeroSuggestFieldTrial() ||
      service == NULL ||
      !service->IsSyncEnabledAndLoggedIn() ||
      !sync_prefs.HasKeepEverythingSynced()) {
    return false;
  }
  return true;
}

bool ZeroSuggestProvider::ShouldSendURL(const GURL& url) const {
  if (!url.is_valid())
    return false;

  // Only allow HTTP URLs or Google HTTPS URLs (including Google search
  // result pages).  For the latter case, Google was already sent the HTTPS
  // URLs when requesting the page, so the information is just re-sent.
  return (url.scheme() == content::kHttpScheme) ||
      google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS);
}

void ZeroSuggestProvider::FillResults(
    const Value& root_val,
    int* verbatim_relevance,
    SearchProvider::SuggestResults* suggest_results,
    SearchProvider::NavigationResults* navigation_results) {
  string16 query;
  const ListValue* root_list = NULL;
  const ListValue* results = NULL;
  const ListValue* relevances = NULL;
  // The response includes the query, which should be empty for ZeroSuggest
  // responses.
  if (!root_val.GetAsList(&root_list) || !root_list->GetString(0, &query) ||
      (!query.empty()) || !root_list->GetList(1, &results))
    return;

  // 3rd element: Description list.
  const ListValue* descriptions = NULL;
  root_list->GetList(2, &descriptions);

  // 4th element: Disregard the query URL list for now.

  // Reset suggested relevance information from the provider.
  *verbatim_relevance = kDefaultVerbatimZeroSuggestRelevance;

  // 5th element: Optional key-value pairs from the Suggest server.
  const ListValue* types = NULL;
  const DictionaryValue* extras = NULL;
  if (root_list->GetDictionary(4, &extras)) {
    extras->GetList("google:suggesttype", &types);

    // Discard this list if its size does not match that of the suggestions.
    if (extras->GetList("google:suggestrelevance", &relevances) &&
        relevances->GetSize() != results->GetSize())
      relevances = NULL;
    extras->GetInteger("google:verbatimrelevance", verbatim_relevance);

    // Check if the active suggest field trial (if any) has triggered.
    bool triggered = false;
    extras->GetBoolean("google:fieldtrialtriggered", &triggered);
    field_trial_triggered_ |= triggered;
    field_trial_triggered_in_session_ |= triggered;
  }

  // Clear the previous results now that new results are available.
  suggest_results->clear();
  navigation_results->clear();

  string16 result, title;
  std::string type;
  for (size_t index = 0; results->GetString(index, &result); ++index) {
    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    if (result.empty())
      continue;

    int relevance = kDefaultZeroSuggestRelevance;

    // Apply valid suggested relevance scores; discard invalid lists.
    if (relevances != NULL && !relevances->GetInteger(index, &relevance))
      relevances = NULL;
    if (types && types->GetString(index, &type) && (type == "NAVIGATION")) {
      // Do not blindly trust the URL coming from the server to be valid.
      GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(result), std::string()));
      if (url.is_valid()) {
        if (descriptions != NULL)
          descriptions->GetString(index, &title);
        navigation_results->push_back(SearchProvider::NavigationResult(
            *this, url, title, false, relevance, relevances != NULL));
      }
    } else {
      suggest_results->push_back(SearchProvider::SuggestResult(
          result, false, relevance, relevances != NULL, false));
    }
  }
}

void ZeroSuggestProvider::AddSuggestResultsToMap(
    const SearchProvider::SuggestResults& results,
    const TemplateURL* template_url,
    SearchProvider::MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i) {
    AddMatchToMap(results[i].relevance(), AutocompleteMatchType::SEARCH_SUGGEST,
                  template_url, results[i].suggestion(), i, map);
  }
}

void ZeroSuggestProvider::AddMatchToMap(int relevance,
                                        AutocompleteMatch::Type type,
                                        const TemplateURL* template_url,
                                        const string16& query_string,
                                        int accepted_suggestion,
                                        SearchProvider::MatchMap* map) {
  // Pass in query_string as the input_text since we don't want any bolding.
  // TODO(samarth|melevin): use the actual omnibox margin here as well instead
  // of passing in -1.
  AutocompleteMatch match = SearchProvider::CreateSearchSuggestion(
      this, relevance, type, template_url, query_string, query_string,
      AutocompleteInput(), false, accepted_suggestion, -1, true);
  if (!match.destination_url.is_valid())
    return;

  // Try to add |match| to |map|.  If a match for |query_string| is already in
  // |map|, replace it if |match| is more relevant.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  const std::pair<SearchProvider::MatchMap::iterator, bool> i(map->insert(
      std::make_pair(base::i18n::ToLower(query_string), match)));
  // NOTE: We purposefully do a direct relevance comparison here instead of
  // using AutocompleteMatch::MoreRelevant(), so that we'll prefer "items added
  // first" rather than "items alphabetically first" when the scores are equal.
  // The only case this matters is when a user has results with the same score
  // that differ only by capitalization; because the history system returns
  // results sorted by recency, this means we'll pick the most recent such
  // result even if the precision of our relevance score is too low to
  // distinguish the two.
  if (!i.second && (match.relevance > i.first->second.relevance))
    i.first->second = match;
}

AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    const SearchProvider::NavigationResult& navigation) {
  AutocompleteMatch match(this, navigation.relevance(), false,
                          AutocompleteMatchType::NAVSUGGEST);
  match.destination_url = navigation.url();

  const std::string languages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  match.contents = net::FormatUrl(navigation.url(), languages,
      net::kFormatUrlOmitAll, net::UnescapeRule::SPACES, NULL, NULL, NULL);
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(navigation.url(),
          match.contents);

  AutocompleteMatch::ClassifyLocationInString(string16::npos, 0,
      match.contents.length(), ACMatchClassification::URL,
      &match.contents_class);

  match.description =
      AutocompleteMatch::SanitizeString(navigation.description());
  AutocompleteMatch::ClassifyLocationInString(string16::npos, 0,
      match.description.length(), ACMatchClassification::NONE,
      &match.description_class);
  return match;
}

void ZeroSuggestProvider::Run() {
  have_pending_request_ = false;
  const int kFetcherID = 1;

  const TemplateURL* default_provider =
     template_url_service_->GetDefaultSearchProvider();
  // TODO(hfung): Generalize if the default provider supports zero suggest.
  // Only make the request if we know that the provider supports zero suggest
  // (currently only the prepopulated Google provider).
  if (default_provider == NULL || !default_provider->SupportsReplacement() ||
      default_provider->prepopulate_id() != 1) {
    Stop(true);
    return;
  }
  string16 prefix;
  TemplateURLRef::SearchTermsArgs search_term_args(prefix);
  search_term_args.zero_prefix_url = current_query_;
  std::string req_url = default_provider->suggestions_url_ref().
      ReplaceSearchTerms(search_term_args);
  GURL suggest_url(req_url);
  // Make sure we are sending the suggest request through HTTPS.
  if (!suggest_url.SchemeIs(content::kHttpsScheme)) {
    Stop(true);
    return;
  }

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
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
  have_pending_request_ = true;
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_SENT);
}

void ZeroSuggestProvider::ParseSuggestResults(const Value& root_val) {
  SearchProvider::SuggestResults suggest_results;
  FillResults(root_val, &verbatim_relevance_,
              &suggest_results, &navigation_results_);

  query_matches_map_.clear();
  AddSuggestResultsToMap(suggest_results,
                         template_url_service_->GetDefaultSearchProvider(),
                         &query_matches_map_);
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

  const int num_query_results = query_matches_map_.size();
  const int num_nav_results = navigation_results_.size();
  const int num_results = num_query_results + num_nav_results;
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.QueryResults", num_query_results);
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.URLResults",  num_nav_results);
  UMA_HISTOGRAM_COUNTS("ZeroSuggest.AllResults", num_results);

  // Show Most Visited results after ZeroSuggest response is received.
  if (OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial()) {
    matches_.push_back(current_url_match_);
    int relevance = 600;
    if (num_results > 0) {
      UMA_HISTOGRAM_COUNTS(
          "Omnibox.ZeroSuggest.MostVisitedResultsCounterfactual",
          most_visited_urls_.size());
    }
    for (size_t i = 0; i < most_visited_urls_.size(); i++) {
      const history::MostVisitedURL& url = most_visited_urls_[i];
      SearchProvider::NavigationResult nav(*this, url.url, url.title, false,
                                           relevance, true);
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

  for (SearchProvider::MatchMap::const_iterator it(query_matches_map_.begin());
       it != query_matches_map_.end(); ++it)
    matches_.push_back(it->second);

  for (SearchProvider::NavigationResults::const_iterator it(
       navigation_results_.begin()); it != navigation_results_.end(); ++it)
    matches_.push_back(NavigationToMatch(*it));
}

AutocompleteMatch ZeroSuggestProvider::MatchForCurrentURL() {
  AutocompleteInput input(permanent_text_, string16::npos, string16(),
                          GURL(current_query_), current_page_classification_,
                          false, false, true, AutocompleteInput::ALL_MATCHES);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
      permanent_text_, false, true, &match, NULL);
  match.is_history_what_you_typed_match = false;
  match.allowed_to_be_default_match = true;

  // The placeholder suggestion for the current URL has high relevance so
  // that it is in the first suggestion slot and inline autocompleted. It
  // gets dropped as soon as the user types something.
  match.relevance = verbatim_relevance_;

  return match;
}
