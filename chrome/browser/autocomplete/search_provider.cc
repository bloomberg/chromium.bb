// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/search_provider.h"

#include <algorithm>
#include <cmath>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

using base::Time;
using base::TimeDelta;

namespace {

// We keep track in a histogram how many suggest requests we send, how
// many suggest requests we invalidate (e.g., due to a user typing
// another character), and how many replies we receive.
// *** ADD NEW ENUMS AFTER ALL PREVIOUSLY DEFINED ONES! ***
//     (excluding the end-of-list enum value)
// We do not want values of existing enums to change or else it screws
// up the statistics.
enum SuggestRequestsHistogramValue {
  REQUEST_SENT = 1,
  REQUEST_INVALIDATED,
  REPLY_RECEIVED,
  MAX_SUGGEST_REQUEST_HISTOGRAM_VALUE
};

// The verbatim score for an input which is not an URL.
const int kNonURLVerbatimRelevance = 1300;

// Increments the appropriate value in the histogram by one.
void LogOmniboxSuggestRequest(
    SuggestRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.SuggestRequests", request_value,
                            MAX_SUGGEST_REQUEST_HISTOGRAM_VALUE);
}

bool HasMultipleWords(const string16& text) {
  base::i18n::BreakIterator i(text, base::i18n::BreakIterator::BREAK_WORD);
  bool found_word = false;
  if (i.Init()) {
    while (i.Advance()) {
      if (i.IsWord()) {
        if (found_word)
          return true;
        found_word = true;
      }
    }
  }
  return false;
}

}  // namespace


// SearchProvider::Providers --------------------------------------------------

SearchProvider::Providers::Providers(TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
}

const TemplateURL* SearchProvider::Providers::GetDefaultProviderURL() const {
  return default_provider_.empty() ? NULL :
      template_url_service_->GetTemplateURLForKeyword(default_provider_);
}

const TemplateURL* SearchProvider::Providers::GetKeywordProviderURL() const {
  return keyword_provider_.empty() ? NULL :
      template_url_service_->GetTemplateURLForKeyword(keyword_provider_);
}


// SearchProvider -------------------------------------------------------------

// static
const int SearchProvider::kDefaultProviderURLFetcherID = 1;
// static
const int SearchProvider::kKeywordProviderURLFetcherID = 2;

SearchProvider::SearchProvider(AutocompleteProviderListener* listener,
                               Profile* profile)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_SEARCH),
      providers_(TemplateURLServiceFactory::GetForProfile(profile)),
      suggest_results_pending_(0),
      has_default_suggested_relevance_(false),
      has_keyword_suggested_relevance_(false),
      default_verbatim_relevance_(-1),
      keyword_verbatim_relevance_(-1),
      have_suggest_results_(false),
      instant_finalized_(false),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false) {
}

void SearchProvider::FinalizeInstantQuery(const string16& input_text,
                                          const InstantSuggestion& suggestion) {
  if (done_ || instant_finalized_)
    return;

  instant_finalized_ = true;
  UpdateDone();

  if (input_text.empty()) {
    // We only need to update the listener if we're actually done.
    if (done_)
      listener_->OnProviderUpdate(false);
    return;
  }

  default_provider_suggestion_ = suggestion;

  string16 adjusted_input_text(input_text);
  AutocompleteInput::RemoveForcedQueryStringIfNecessary(input_.type(),
                                                        &adjusted_input_text);

  const string16 text = adjusted_input_text + suggestion.text;
  bool results_updated = false;
  // Remove any matches that are identical to |text|. We don't use the
  // destination_url for comparison as it varies depending upon the index passed
  // to TemplateURL::ReplaceSearchTerms.
  for (ACMatches::iterator i = matches_.begin(); i != matches_.end();) {
    if (((i->type == AutocompleteMatch::SEARCH_HISTORY) ||
         (i->type == AutocompleteMatch::SEARCH_SUGGEST)) &&
        (i->fill_into_edit == text)) {
      i = matches_.erase(i);
      results_updated = true;
    } else {
      ++i;
    }
  }

  // Add the new Instant suggest result.
  if (suggestion.type == INSTANT_SUGGESTION_SEARCH) {
    // Instant has a query suggestion. Rank it higher than SEARCH_WHAT_YOU_TYPED
    // so that it gets autocompleted.
    const int verbatim_relevance = GetVerbatimRelevance();
    int did_not_accept_default_suggestion = default_suggest_results_.empty() ?
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
        TemplateURLRef::NO_SUGGESTION_CHOSEN;
    MatchMap match_map;
    AddMatchToMap(text, adjusted_input_text, verbatim_relevance + 1,
                  AutocompleteMatch::SEARCH_SUGGEST,
                  did_not_accept_default_suggestion, false, &match_map);
    if (!match_map.empty()) {
      matches_.push_back(match_map.begin()->second);
      results_updated = true;
    }
  } else {
    // Instant has a URL suggestion. Rank it higher than URL_WHAT_YOU_TYPED so
    // it gets autocompleted; use kNonURLVerbatimRelevance rather than
    // verbatim_relevance so that the score does not change if the user keeps
    // typing and the input changes from type UNKNOWN to URL.
    matches_.push_back(NavigationToMatch(
        NavigationResult(GURL(UTF16ToUTF8(suggestion.text)),
                         string16(),
                         false,
                         kNonURLVerbatimRelevance + 1)));
    results_updated = true;
  }

  if (results_updated || done_)
    listener_->OnProviderUpdate(results_updated);
}

void SearchProvider::ClearInstantSuggestion() {
  default_provider_suggestion_ = InstantSuggestion();
  if (done_ || instant_finalized_)
    return;
  instant_finalized_ = true;
  UpdateMatches();
  listener_->OnProviderUpdate(true);
}

void SearchProvider::Start(const AutocompleteInput& input,
                           bool minimal_changes) {
  // Do our best to load the model as early as possible.  This will reduce
  // odds of having the model not ready when really needed (a non-empty input).
  TemplateURLService* model = providers_.template_url_service();
  DCHECK(model);
  model->Load();

  matches_.clear();
  field_trial_triggered_ = false;

  instant_finalized_ =
      (input.matches_requested() != AutocompleteInput::ALL_MATCHES);

  // Can't return search/suggest results for bogus input or without a profile.
  if (!profile_ || (input.type() == AutocompleteInput::INVALID)) {
    Stop(false);
    return;
  }

  keyword_input_ = input;
  const TemplateURL* keyword_provider =
      KeywordProvider::GetSubstitutingTemplateURLForInput(model,
                                                          &keyword_input_);
  if (keyword_provider == NULL)
    keyword_input_.Clear();
  else if (keyword_input_.text().empty())
    keyword_provider = NULL;

  const TemplateURL* default_provider = model->GetDefaultSearchProvider();
  if (default_provider && !default_provider->SupportsReplacement())
    default_provider = NULL;

  if (keyword_provider == default_provider)
    default_provider = NULL;  // No use in querying the same provider twice.

  if (!default_provider && !keyword_provider) {
    // No valid providers.
    Stop(false);
    return;
  }

  // If we're still running an old query but have since changed the query text
  // or the providers, abort the query.
  string16 default_provider_keyword(default_provider ?
      default_provider->keyword() : string16());
  string16 keyword_provider_keyword(keyword_provider ?
      keyword_provider->keyword() : string16());
  if (!minimal_changes ||
      !providers_.equal(default_provider_keyword, keyword_provider_keyword)) {
    // If Instant has not come back with a suggestion, adjust the previous
    // suggestion if possible. If |instant_finalized| is true, we are looking
    // for synchronous matches only, so the suggestion is cleared.
    if (instant_finalized_)
      default_provider_suggestion_ = InstantSuggestion();
    else
      AdjustDefaultProviderSuggestion(input_.text(), input.text());

    // Cancel any in-flight suggest requests.
    if (!done_) {
      // The Stop(false) call below clears |default_provider_suggestion_|, but
      // in this instance we do not want to clear cached results, so we
      // restore it.
      base::AutoReset<InstantSuggestion> reset(&default_provider_suggestion_,
                                               InstantSuggestion());
      Stop(false);
    }
  }

  providers_.set(default_provider_keyword, keyword_provider_keyword);

  if (input.text().empty()) {
    // User typed "?" alone.  Give them a placeholder result indicating what
    // this syntax does.
    if (default_provider) {
      AutocompleteMatch match;
      match.provider = this;
      match.contents.assign(l10n_util::GetStringUTF16(IDS_EMPTY_KEYWORD_VALUE));
      match.contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::NONE));
      match.keyword = providers_.default_provider();
      matches_.push_back(match);
    }
    Stop(false);
    return;
  }

  input_ = input;

  // When Instant is enabled in the extended mode, the embedded page will handle
  // all search suggestions itself, so don't run the normal provider flow.
  // TODO(dcblack): Once we are done refactoring the omnibox so we don't need to
  // use FinalizeInstantQuery anymore, we can take out this check and remove
  // this provider from kInstantExtendedOmniboxProviders.
  if (!chrome::IsInstantExtendedAPIEnabled() ||
      !chrome::IsInstantEnabled(profile_)) {
    DoHistoryQuery(minimal_changes);
    StartOrStopSuggestQuery(minimal_changes);
  }
  UpdateMatches();
}

SearchProvider::Result::Result(bool from_keyword_provider, int relevance)
    : from_keyword_provider_(from_keyword_provider),
      relevance_(relevance) {
}

SearchProvider::Result::~Result() {}

SearchProvider::SuggestResult::SuggestResult(const string16& suggestion,
                                             bool from_keyword_provider,
                                             int relevance)
    : Result(from_keyword_provider, relevance),
      suggestion_(suggestion) {
}

SearchProvider::SuggestResult::~SuggestResult() {}

int SearchProvider::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_provider_ && keyword_provider_requested)
    return 100;
  return ((input.type() == AutocompleteInput::URL) ? 300 : 600);
}

SearchProvider::NavigationResult::NavigationResult(
    const GURL& url,
    const string16& description,
    bool from_keyword_provider,
    int relevance)
    : Result(from_keyword_provider, relevance),
      url_(url),
      description_(description) {
  DCHECK(url_.is_valid());
}

int SearchProvider::NavigationResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  return (from_keyword_provider_ || !keyword_provider_requested) ? 800 : 150;
}

SearchProvider::NavigationResult::~NavigationResult() {}

class SearchProvider::CompareScoredResults {
 public:
  bool operator()(const Result& a, const Result& b) {
    // Sort in descending relevance order.
    return a.relevance() > b.relevance();
  }
};

void SearchProvider::Run() {
  // Start a new request with the current input.
  suggest_results_pending_ = 0;
  time_suggest_request_sent_ = base::TimeTicks::Now();

  default_fetcher_.reset(CreateSuggestFetcher(kDefaultProviderURLFetcherID,
      providers_.GetDefaultProviderURL(), input_));
  keyword_fetcher_.reset(CreateSuggestFetcher(kKeywordProviderURLFetcherID,
      providers_.GetKeywordProviderURL(), keyword_input_));

  // Both the above can fail if the providers have been modified or deleted
  // since the query began.
  if (suggest_results_pending_ == 0) {
    UpdateDone();
    // We only need to update the listener if we're actually done.
    if (done_)
      listener_->OnProviderUpdate(false);
  }
}

void SearchProvider::Stop(bool clear_cached_results) {
  StopSuggest();
  done_ = true;
  default_provider_suggestion_ = InstantSuggestion();

  if (clear_cached_results)
    ClearResults();
}

void SearchProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  uint32 field_trial_hash = 0;
  if (OmniboxFieldTrial::GetActiveSuggestFieldTrialHash(&field_trial_hash)) {
    if (field_trial_triggered_)
      new_entry.mutable_field_trial_triggered()->Add(field_trial_hash);
    if (field_trial_triggered_in_session_) {
      new_entry.mutable_field_trial_triggered_in_session()->Add(
          field_trial_hash);
    }
  }
}

void SearchProvider::ResetSession() {
  field_trial_triggered_in_session_ = false;
}

void SearchProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!done_);
  suggest_results_pending_--;
  LogOmniboxSuggestRequest(REPLY_RECEIVED);
  DCHECK_GE(suggest_results_pending_, 0);  // Should never go negative.
  const net::HttpResponseHeaders* const response_headers =
      source->GetResponseHeaders();
  std::string json_data;
  source->GetResponseAsString(&json_data);
  // JSON is supposed to be UTF-8, but some suggest service providers send JSON
  // files in non-UTF-8 encodings.  The actual encoding is usually specified in
  // the Content-Type header field.
  if (response_headers) {
    std::string charset;
    if (response_headers->GetCharset(&charset)) {
      string16 data_16;
      // TODO(jungshik): Switch to CodePageToUTF8 after it's added.
      if (base::CodepageToUTF16(json_data, charset.c_str(),
                                base::OnStringConversionError::FAIL,
                                &data_16))
        json_data = UTF16ToUTF8(data_16);
    }
  }

  const bool is_keyword = (source == keyword_fetcher_.get());
  // Ensure the request succeeded and that the provider used is still available.
  // A verbatim match cannot be generated without this provider, causing errors.
  const bool request_succeeded =
      source->GetStatus().is_success() && source->GetResponseCode() == 200 &&
      ((is_keyword && providers_.GetKeywordProviderURL()) ||
       (!is_keyword && providers_.GetDefaultProviderURL()));

  // Record response time for suggest requests sent to Google.  We care
  // only about the common case: the Google default provider used in
  // non-keyword mode.
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  if (!is_keyword && default_url &&
      (TemplateURLPrepopulateData::GetEngineType(default_url->url()) ==
       SEARCH_ENGINE_GOOGLE)) {
    const TimeDelta elapsed_time =
        base::TimeTicks::Now() - time_suggest_request_sent_;
    if (request_succeeded) {
      UMA_HISTOGRAM_TIMES("Omnibox.SuggestRequest.Success.GoogleResponseTime",
                          elapsed_time);
    } else {
      UMA_HISTOGRAM_TIMES("Omnibox.SuggestRequest.Failure.GoogleResponseTime",
                          elapsed_time);
    }
  }

  bool results_updated = false;
  if (request_succeeded) {
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> data(deserializer.Deserialize(NULL, NULL));
    results_updated = data.get() && ParseSuggestResults(data.get(), is_keyword);
  }

  UpdateMatches();
  if (done_ || results_updated)
    listener_->OnProviderUpdate(results_updated);
}

SearchProvider::~SearchProvider() {
}

void SearchProvider::DoHistoryQuery(bool minimal_changes) {
  // The history query results are synchronous, so if minimal_changes is true,
  // we still have the last results and don't need to do anything.
  if (minimal_changes)
    return;

  keyword_history_results_.clear();
  default_history_results_.clear();

  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  history::URLDatabase* url_db = history_service ?
      history_service->InMemoryDatabase() : NULL;
  if (!url_db)
    return;

  // Request history for both the keyword and default provider.  We grab many
  // more matches than we'll ultimately clamp to so that if there are several
  // recent multi-word matches who scores are lowered (see
  // AddHistoryResultsToMap()), they won't crowd out older, higher-scoring
  // matches.  Note that this doesn't fix the problem entirely, but merely
  // limits it to cases with a very large number of such multi-word matches; for
  // now, this seems OK compared with the complexity of a real fix, which would
  // require multiple searches and tracking of "single- vs. multi-word" in the
  // database.
  int num_matches = kMaxMatches * 5;
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  if (default_url) {
    url_db->GetMostRecentKeywordSearchTerms(default_url->id(), input_.text(),
        num_matches, &default_history_results_);
  }
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  if (keyword_url) {
    url_db->GetMostRecentKeywordSearchTerms(keyword_url->id(),
        keyword_input_.text(), num_matches, &keyword_history_results_);
  }
}

void SearchProvider::StartOrStopSuggestQuery(bool minimal_changes) {
  if (!IsQuerySuitableForSuggest()) {
    StopSuggest();
    ClearResults();
    return;
  }

  // For the minimal_changes case, if we finished the previous query and still
  // have its results, or are allowed to keep running it, just do that, rather
  // than starting a new query.
  if (minimal_changes &&
      (have_suggest_results_ ||
       (!done_ &&
        input_.matches_requested() == AutocompleteInput::ALL_MATCHES)))
    return;

  // We can't keep running any previous query, so halt it.
  StopSuggest();

  // Remove existing results that cannot inline autocomplete the new input.
  RemoveStaleResults();

  // We can't start a new query if we're only allowed synchronous results.
  if (input_.matches_requested() != AutocompleteInput::ALL_MATCHES)
    return;

  // To avoid flooding the suggest server, don't send a query until at least 100
  // ms since the last query.
  const int kMinimumTimeBetweenSuggestQueriesMs = 100;
  base::TimeTicks next_suggest_time(time_suggest_request_sent_ +
      TimeDelta::FromMilliseconds(kMinimumTimeBetweenSuggestQueriesMs));
  base::TimeTicks now(base::TimeTicks::Now());
  if (now >= next_suggest_time) {
    Run();
    return;
  }
  timer_.Start(FROM_HERE, next_suggest_time - now, this, &SearchProvider::Run);
}

bool SearchProvider::IsQuerySuitableForSuggest() const {
  // Don't run Suggest in incognito mode, if the engine doesn't support it, or
  // if the user has disabled it.
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  if (profile_->IsOffTheRecord() ||
      ((!default_url || default_url->suggestions_url().empty()) &&
       (!keyword_url || keyword_url->suggestions_url().empty())) ||
      !profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled))
    return false;

  // If the input type might be a URL, we take extra care so that private data
  // isn't sent to the server.

  // FORCED_QUERY means the user is explicitly asking us to search for this, so
  // we assume it isn't a URL and/or there isn't private data.
  if (input_.type() == AutocompleteInput::FORCED_QUERY)
    return true;

  // Next we check the scheme.  If this is UNKNOWN/URL with a scheme that isn't
  // http/https/ftp, we shouldn't send it.  Sending things like file: and data:
  // is both a waste of time and a disclosure of potentially private, local
  // data.  Other "schemes" may actually be usernames, and we don't want to send
  // passwords.  If the scheme is OK, we still need to check other cases below.
  // If this is QUERY, then the presence of these schemes means the user
  // explicitly typed one, and thus this is probably a URL that's being entered
  // and happens to currently be invalid -- in which case we again want to run
  // our checks below.  Other QUERY cases are less likely to be URLs and thus we
  // assume we're OK.
  if (!LowerCaseEqualsASCII(input_.scheme(), chrome::kHttpScheme) &&
      !LowerCaseEqualsASCII(input_.scheme(), chrome::kHttpsScheme) &&
      !LowerCaseEqualsASCII(input_.scheme(), chrome::kFtpScheme))
    return (input_.type() == AutocompleteInput::QUERY);

  // Don't send URLs with usernames, queries or refs.  Some of these are
  // private, and the Suggest server is unlikely to have any useful results
  // for any of them.  Also don't send URLs with ports, as we may initially
  // think that a username + password is a host + port (and we don't want to
  // send usernames/passwords), and even if the port really is a port, the
  // server is once again unlikely to have and useful results.
  const url_parse::Parsed& parts = input_.parts();
  if (parts.username.is_nonempty() || parts.port.is_nonempty() ||
      parts.query.is_nonempty() || parts.ref.is_nonempty())
    return false;

  // Don't send anything for https except the hostname.  Hostnames are OK
  // because they are visible when the TCP connection is established, but the
  // specific path may reveal private information.
  if (LowerCaseEqualsASCII(input_.scheme(), chrome::kHttpsScheme) &&
      parts.path.is_nonempty())
    return false;

  return true;
}

void SearchProvider::StopSuggest() {
  // Increment the appropriate field in the histogram by the number of
  // pending requests that were invalidated.
  for (int i = 0; i < suggest_results_pending_; i++)
    LogOmniboxSuggestRequest(REQUEST_INVALIDATED);
  suggest_results_pending_ = 0;
  timer_.Stop();
  // Stop any in-progress URL fetches.
  keyword_fetcher_.reset();
  default_fetcher_.reset();
}

void SearchProvider::ClearResults() {
  keyword_suggest_results_.clear();
  default_suggest_results_.clear();
  keyword_navigation_results_.clear();
  default_navigation_results_.clear();
  has_default_suggested_relevance_ = false;
  has_keyword_suggested_relevance_ = false;
  default_verbatim_relevance_ = -1;
  keyword_verbatim_relevance_ = -1;
  have_suggest_results_ = false;
}

void SearchProvider::RemoveStaleResults() {
  // Keyword provider results should match |keyword_input_.text()|, unless
  // the input was just changed to non-keyword mode; in that case, compare
  // against |input_.text()|.
  const string16& keyword_input =
      !keyword_input_.text().empty() ? keyword_input_.text() : input_.text();
  RemoveStaleSuggestResults(&keyword_suggest_results_, keyword_input);
  RemoveStaleSuggestResults(&default_suggest_results_, input_.text());
  RemoveStaleNavigationResults(&keyword_navigation_results_, keyword_input);
  RemoveStaleNavigationResults(&default_navigation_results_, input_.text());
}

// static
void SearchProvider::RemoveStaleSuggestResults(SuggestResults* list,
                                               const string16& input) {
  for (SuggestResults::iterator i = list->begin(); i < list->end();)
    i = StartsWith(i->suggestion(), input, false) ? (i + 1) : list->erase(i);
}

void SearchProvider::RemoveStaleNavigationResults(NavigationResults* list,
                                                  const string16& input) {
  for (NavigationResults::iterator i = list->begin(); i < list->end();) {
    const string16 fill(AutocompleteInput::FormattedStringWithEquivalentMeaning(
        i->url(), StringForURLDisplay(i->url(), true, false)));
    i = URLPrefix::BestURLPrefix(fill, input) ? (i + 1) : list->erase(i);
  }
}

void SearchProvider::AdjustDefaultProviderSuggestion(
    const string16& previous_input,
    const string16& current_input) {
  if (default_provider_suggestion_.type == INSTANT_SUGGESTION_URL) {
    // Build a list of NavigationResults with only one NavigationResult in it,
    // initialized with the URL in the navigation suggestion. This allows the
    // use of RemoveStaleNavigationResults(), which has non-trivial logic to
    // determine staleness.
    NavigationResults list;
    // Description and relevance do not matter in the check for staleness.
    NavigationResult result(GURL(default_provider_suggestion_.text),
                            string16(),
                            false,
                            100);
    list.push_back(result);
    RemoveStaleNavigationResults(&list, current_input);
    // If navigation suggestion is stale, clear |default_provider_suggestion_|.
    if (list.empty())
      default_provider_suggestion_ = InstantSuggestion();
  } else {
    DCHECK(default_provider_suggestion_.type == INSTANT_SUGGESTION_SEARCH);
    // InstantSuggestion of type SEARCH contain only the suggested text, and not
    // the full text of the query. This looks at the current and previous input
    // to determine if the user is typing forward, and if the new input is
    // contained in |default_provider_suggestion_|. If so, the suggestion is
    // adjusted and can be kept. Otherwise, it is reset.
    if (!previous_input.empty() &&
        StartsWith(current_input, previous_input, false)) {
      // User is typing forward; verify if new input is part of the suggestion.
      const string16 new_text = string16(current_input, previous_input.size());
      if (StartsWith(default_provider_suggestion_.text, new_text, false)) {
        // New input is a prefix to the previous suggestion, adjust the
        // suggestion to strip the prefix.
        default_provider_suggestion_.text.erase(0, new_text.size());
        return;
      }
    }
    // If we are here, the search suggestion is stale; reset it.
    default_provider_suggestion_ = InstantSuggestion();
  }
}

void SearchProvider::ApplyCalculatedRelevance() {
  ApplyCalculatedSuggestRelevance(&keyword_suggest_results_);
  ApplyCalculatedSuggestRelevance(&default_suggest_results_);
  ApplyCalculatedNavigationRelevance(&keyword_navigation_results_);
  ApplyCalculatedNavigationRelevance(&default_navigation_results_);
  has_default_suggested_relevance_ = false;
  has_keyword_suggested_relevance_ = false;
  default_verbatim_relevance_ = -1;
  keyword_verbatim_relevance_ = -1;
}

void SearchProvider::ApplyCalculatedSuggestRelevance(SuggestResults* list) {
  for (size_t i = 0; i < list->size(); ++i) {
    SuggestResult& result = (*list)[i];
    result.set_relevance(
        result.CalculateRelevance(input_, providers_.has_keyword_provider()) +
        (list->size() - i - 1));
  }
}

void SearchProvider::ApplyCalculatedNavigationRelevance(
    NavigationResults* list) {
  for (size_t i = 0; i < list->size(); ++i) {
    NavigationResult& result = (*list)[i];
    result.set_relevance(
        result.CalculateRelevance(input_, providers_.has_keyword_provider()) +
        (list->size() - i - 1));
  }
}

net::URLFetcher* SearchProvider::CreateSuggestFetcher(
    int id,
    const TemplateURL* template_url,
    const AutocompleteInput& input) {
  if (!template_url || template_url->suggestions_url().empty())
    return NULL;

  // Bail if the suggestion URL is invalid with the given replacements.
  TemplateURLRef::SearchTermsArgs search_term_args(input.text());
  search_term_args.cursor_position = input.cursor_position();
  GURL suggest_url(template_url->suggestions_url_ref().ReplaceSearchTerms(
      search_term_args));
  if (!suggest_url.is_valid())
    return NULL;

  suggest_results_pending_++;
  LogOmniboxSuggestRequest(REQUEST_SENT);

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(id, suggest_url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(profile_->GetRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  chrome_variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      fetcher->GetOriginalURL(), profile_->IsOffTheRecord(), false, &headers);
  fetcher->SetExtraRequestHeaders(headers.ToString());
  fetcher->Start();
  return fetcher;
}

bool SearchProvider::ParseSuggestResults(Value* root_val, bool is_keyword) {
  // TODO(pkasting): Fix |have_suggest_results_|; see http://crbug.com/130631
  have_suggest_results_ = false;

  string16 query;
  ListValue* root_list = NULL;
  ListValue* results = NULL;
  const string16& input_text =
      is_keyword ? keyword_input_.text() : input_.text();
  if (!root_val->GetAsList(&root_list) || !root_list->GetString(0, &query) ||
      (query != input_text) || !root_list->GetList(1, &results))
    return false;

  // 3rd element: Description list.
  ListValue* descriptions = NULL;
  root_list->GetList(2, &descriptions);

  // 4th element: Disregard the query URL list for now.

  // Reset suggested relevance information from the default provider.
  bool* has_suggested_relevance = is_keyword ?
      &has_keyword_suggested_relevance_ : &has_default_suggested_relevance_;
  *has_suggested_relevance = false;
  int* verbatim_relevance = is_keyword ?
      &keyword_verbatim_relevance_ : &default_verbatim_relevance_;
  *verbatim_relevance = -1;

  // 5th element: Optional key-value pairs from the Suggest server.
  ListValue* types = NULL;
  ListValue* relevances = NULL;
  DictionaryValue* extras = NULL;
  if (root_list->GetDictionary(4, &extras)) {
    extras->GetList("google:suggesttype", &types);

    // Only accept relevance suggestions if Instant is disabled.
    if (!chrome::IsInstantEnabled(profile_)) {
      // Discard this list if its size does not match that of the suggestions.
      if (extras->GetList("google:suggestrelevance", &relevances) &&
          relevances->GetSize() != results->GetSize())
        relevances = NULL;
      extras->GetInteger("google:verbatimrelevance", verbatim_relevance);
    }

    // Check if the active suggest field trial (if any) has triggered either
    // for the default provider or keyword provider.
    bool triggered = false;
    extras->GetBoolean("google:fieldtrialtriggered", &triggered);
    field_trial_triggered_ |= triggered;
    field_trial_triggered_in_session_ |= triggered;
  }

  SuggestResults* suggest_results =
      is_keyword ? &keyword_suggest_results_ : &default_suggest_results_;
  NavigationResults* navigation_results =
      is_keyword ? &keyword_navigation_results_ : &default_navigation_results_;

  // Clear the previous results now that new results are available.
  suggest_results->clear();
  navigation_results->clear();

  string16 result, title;
  std::string type;
  int relevance = -1;
  for (size_t index = 0; results->GetString(index, &result); ++index) {
    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    if (result.empty())
      continue;

    // Apply valid suggested relevance scores; discard invalid lists.
    if (relevances != NULL && !relevances->GetInteger(index, &relevance))
      relevances = NULL;
    if (types && types->GetString(index, &type) && (type == "NAVIGATION")) {
      // Do not blindly trust the URL coming from the server to be valid.
      GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(result), std::string()));
      if (url.is_valid()) {
        if (descriptions != NULL)
          descriptions->GetString(index, &title);
        navigation_results->push_back(NavigationResult(
            url, title, is_keyword, relevance));
      }
    } else {
      // TODO(kochi): Improve calculator result presentation.
      suggest_results->push_back(SuggestResult(result, is_keyword, relevance));
    }
  }

  // In keyword mode with suggested relevance scores, demote navsuggestions
  // below the top query match (what-you-typed or suggestion).  (We
  // don't want keyword navsuggestions to be inlined.)
  if (is_keyword && (relevances != NULL)) {
    // If we fail to demote navigational result relevance scores, then
    // cancel suggested relevance scoring for keyword mode entirely.
    if (!DemoteKeywordNavigationMatchesBelowTopQueryMatch())
      relevances = NULL;
  }

  // Apply calculated relevance scores if a valid list was not provided.
  if (relevances == NULL) {
    ApplyCalculatedSuggestRelevance(suggest_results);
    ApplyCalculatedNavigationRelevance(navigation_results);
  } else {
    *has_suggested_relevance = true;
  }

  have_suggest_results_ = true;
  return true;
}

void SearchProvider::ConvertResultsToAutocompleteMatches() {
  // Convert all the results to matches and add them to a map, so we can keep
  // the most relevant match for each result.
  MatchMap map;
  const Time no_time;
  int did_not_accept_keyword_suggestion = keyword_suggest_results_.empty() ?
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
      TemplateURLRef::NO_SUGGESTION_CHOSEN;

  int verbatim_relevance = GetVerbatimRelevance();
  int did_not_accept_default_suggestion = default_suggest_results_.empty() ?
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
      TemplateURLRef::NO_SUGGESTION_CHOSEN;
  if (verbatim_relevance > 0) {
    AddMatchToMap(input_.text(), input_.text(), verbatim_relevance,
                  AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
                  did_not_accept_default_suggestion, false, &map);
  }
  if (!keyword_input_.text().empty()) {
    const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
    // We only create the verbatim search query match for a keyword
    // if it's not an extension keyword.  Extension keywords are handled
    // in KeywordProvider::Start().  (Extensions are complicated...)
    // Note: in this provider, SEARCH_OTHER_ENGINE must correspond
    // to the keyword verbatim search query.  Do not create other matches
    // of type SEARCH_OTHER_ENGINE.
    if (keyword_url && !keyword_url->IsExtensionKeyword()) {
      const int keyword_verbatim_relevance = GetKeywordVerbatimRelevance();
      if (keyword_verbatim_relevance > 0) {
        AddMatchToMap(keyword_input_.text(), keyword_input_.text(),
                      keyword_verbatim_relevance,
                      AutocompleteMatch::SEARCH_OTHER_ENGINE,
                      did_not_accept_keyword_suggestion, true, &map);
      }
    }
  }
  const size_t verbatim_matches_size = map.size();
  if (!default_provider_suggestion_.text.empty() &&
      default_provider_suggestion_.type == INSTANT_SUGGESTION_SEARCH &&
      !input_.prevent_inline_autocomplete())
    AddMatchToMap(input_.text() + default_provider_suggestion_.text,
                  input_.text(), verbatim_relevance + 1,
                  AutocompleteMatch::SEARCH_SUGGEST,
                  did_not_accept_default_suggestion, false, &map);

  AddHistoryResultsToMap(keyword_history_results_, true,
                         did_not_accept_keyword_suggestion, &map);
  AddHistoryResultsToMap(default_history_results_, false,
                         did_not_accept_default_suggestion, &map);

  AddSuggestResultsToMap(keyword_suggest_results_, &map);
  AddSuggestResultsToMap(default_suggest_results_, &map);

  // Now add the most relevant matches from the map to |matches_|.
  matches_.clear();
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches_.push_back(i->second);

  if (!default_provider_suggestion_.text.empty() &&
      default_provider_suggestion_.type == INSTANT_SUGGESTION_URL &&
      !input_.prevent_inline_autocomplete()) {
    // See comment in FinalizeInstantQuery() for why we don't use
    // |verbatim_relevance| here.
    matches_.push_back(NavigationToMatch(
        NavigationResult(GURL(UTF16ToUTF8(default_provider_suggestion_.text)),
                         string16(),
                         false,
                         kNonURLVerbatimRelevance + 1)));
  }
  AddNavigationResultsToMatches(keyword_navigation_results_, true);
  AddNavigationResultsToMatches(default_navigation_results_, false);

  // Allow additional match(es) for verbatim results if present.
  const size_t max_total_matches = kMaxMatches + verbatim_matches_size;
  std::partial_sort(matches_.begin(),
      matches_.begin() + std::min(max_total_matches, matches_.size()),
      matches_.end(), &AutocompleteMatch::MoreRelevant);

  if (matches_.size() > max_total_matches)
    matches_.resize(max_total_matches);
}

bool SearchProvider::IsTopMatchScoreTooLow() const {
  // Here we use CalculateRelevanceForVerbatimIgnoringKeywordModeState()
  // rather than CalculateRelevanceForVerbatim() because the latter returns
  // a very low score (250) if keyword mode is active.  This is because
  // when keyword mode is active the user probably wants the keyword matches,
  // not matches from the default provider.  Hence, we use the version of
  // the function that ignores whether keyword mode is active.  This allows
  // SearchProvider to maintain its contract with the AutocompleteController
  // that it will always provide an inlineable match with a reasonable
  // score.
  return matches_.front().relevance <
      CalculateRelevanceForVerbatimIgnoringKeywordModeState();
}

bool SearchProvider::IsTopMatchHighRankSearchForURL() const {
  return input_.type() == AutocompleteInput::URL &&
         matches_.front().relevance > CalculateRelevanceForVerbatim() &&
         (matches_.front().type == AutocompleteMatch::SEARCH_SUGGEST ||
          matches_.front().type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
          matches_.front().type == AutocompleteMatch::SEARCH_OTHER_ENGINE);
}

bool SearchProvider::IsTopMatchNotInlinable() const {
  // Note: this test assumes the SEARCH_OTHER_ENGINE match corresponds to
  // the verbatim search query on the keyword engine.  SearchProvider should
  // not create any other match of type SEARCH_OTHER_ENGINE.
  return matches_.front().type != AutocompleteMatch::SEARCH_WHAT_YOU_TYPED &&
         matches_.front().type != AutocompleteMatch::URL_WHAT_YOU_TYPED &&
         matches_.front().type != AutocompleteMatch::SEARCH_OTHER_ENGINE &&
         matches_.front().inline_autocomplete_offset == string16::npos &&
         matches_.front().fill_into_edit != input_.text();
}

void SearchProvider::UpdateMatches() {
  ConvertResultsToAutocompleteMatches();

  // Check constraints that may be violated by suggested relevances.
  if (!matches_.empty() &&
      (has_default_suggested_relevance_ || default_verbatim_relevance_ >= 0 ||
       has_keyword_suggested_relevance_ || keyword_verbatim_relevance_ >= 0)) {
    // These two blocks attempt to repair undesirable behavior by suggested
    // relevances with minimal impact, preserving other suggested relevances.
    if (IsTopMatchScoreTooLow()) {
      // Disregard the suggested verbatim relevance if the top score is below
      // the usual verbatim value. For example, a BarProvider may rely on
      // SearchProvider's verbatim or inlineable matches for input "foo" to
      // always outrank its own lowly-ranked non-inlineable "bar" match.
      default_verbatim_relevance_ = -1;
      keyword_verbatim_relevance_ = -1;
      ConvertResultsToAutocompleteMatches();
    }
    if (IsTopMatchHighRankSearchForURL()) {
      // Disregard the suggested search and verbatim relevances if the input
      // type is URL and the top match is a highly-ranked search suggestion.
      // For example, prevent a search for "foo.com" from outranking another
      // provider's navigation for "foo.com" or "foo.com/url_from_history".
      ApplyCalculatedSuggestRelevance(&keyword_suggest_results_);
      ApplyCalculatedSuggestRelevance(&default_suggest_results_);
      default_verbatim_relevance_ = -1;
      keyword_verbatim_relevance_ = -1;
      ConvertResultsToAutocompleteMatches();
    }
    if (IsTopMatchNotInlinable()) {
      // Disregard suggested relevances if the top match is not a verbatim
      // match, inlinable, or URL_WHAT_YOU_TYPED (which may be top match
      // regardless of inlining).  For example, input "foo" should not
      // invoke a search for "bar", which would happen if the "bar" search
      // match outranked all other matches.
      ApplyCalculatedRelevance();
      ConvertResultsToAutocompleteMatches();
    }
    DCHECK(!IsTopMatchScoreTooLow());
    DCHECK(!IsTopMatchHighRankSearchForURL());
    DCHECK(!IsTopMatchNotInlinable());
  }

  UpdateStarredStateOfMatches();
  UpdateDone();
}

void SearchProvider::AddNavigationResultsToMatches(
    const NavigationResults& navigation_results,
    bool is_keyword) {
  if (navigation_results.empty())
    return;

  if (is_keyword ?
      has_keyword_suggested_relevance_ : has_default_suggested_relevance_) {
    for (NavigationResults::const_iterator it = navigation_results.begin();
         it != navigation_results.end(); ++it)
      matches_.push_back(NavigationToMatch(*it));
  } else {
    // Pick one element only in absence of the suggested relevance scores.
    // TODO(kochi|msw): Add more navigational results if they get more
    //                  meaningful relevance values; see http://b/1170574.
    // CompareScoredResults sorts by descending relevance; so use min_element.
    NavigationResults::const_iterator result(
        std::min_element(navigation_results.begin(),
                         navigation_results.end(),
                         CompareScoredResults()));
    matches_.push_back(NavigationToMatch(*result));
  }
}

void SearchProvider::AddHistoryResultsToMap(const HistoryResults& results,
                                            bool is_keyword,
                                            int did_not_accept_suggestion,
                                            MatchMap* map) {
  if (results.empty())
    return;

  bool prevent_inline_autocomplete = input_.prevent_inline_autocomplete() ||
      (input_.type() == AutocompleteInput::URL);
  const string16& input_text =
      is_keyword ? keyword_input_.text() : input_.text();
  bool input_multiple_words = HasMultipleWords(input_text);

  SuggestResults scored_results;
  if (!prevent_inline_autocomplete && input_multiple_words) {
    // ScoreHistoryResults() allows autocompletion of multi-word, 1-visit
    // queries if the input also has multiple words.  But if we were already
    // autocompleting a multi-word, multi-visit query, and the current input is
    // still a prefix of it, then changing the autocompletion suddenly feels
    // wrong.  To detect this case, first score as if only one word has been
    // typed, then check for a best result that is an autocompleted, multi-word
    // query.  If we find one, then just keep that score set.
    scored_results = ScoreHistoryResults(results, prevent_inline_autocomplete,
                                         false, input_text, is_keyword);
    if ((scored_results[0].relevance() <
             AutocompleteResult::kLowestDefaultScore) ||
        !HasMultipleWords(scored_results[0].suggestion()))
      scored_results.clear();  // Didn't detect the case above, score normally.
  }
  if (scored_results.empty())
    scored_results = ScoreHistoryResults(results, prevent_inline_autocomplete,
                                         input_multiple_words, input_text,
                                         is_keyword);
  for (SuggestResults::const_iterator i(scored_results.begin());
       i != scored_results.end(); ++i) {
    AddMatchToMap(i->suggestion(), input_text, i->relevance(),
                  AutocompleteMatch::SEARCH_HISTORY, did_not_accept_suggestion,
                  is_keyword, map);
  }
}

SearchProvider::SuggestResults SearchProvider::ScoreHistoryResults(
    const HistoryResults& results,
    bool base_prevent_inline_autocomplete,
    bool input_multiple_words,
    const string16& input_text,
    bool is_keyword) {
  AutocompleteClassifier* classifier =
      AutocompleteClassifierFactory::GetForProfile(profile_);
  SuggestResults scored_results;
  for (HistoryResults::const_iterator i(results.begin()); i != results.end();
       ++i) {
    // Don't autocomplete multi-word queries that have only been seen once
    // unless the user has typed more than one word.
    bool prevent_inline_autocomplete = base_prevent_inline_autocomplete ||
        (!input_multiple_words && (i->visits < 2) && HasMultipleWords(i->term));

    // Don't autocomplete search terms that would normally be treated as URLs
    // when typed. For example, if the user searched for "google.com" and types
    // "goog", don't autocomplete to the search term "google.com". Otherwise,
    // the input will look like a URL but act like a search, which is confusing.
    // NOTE: We don't check this in the following cases:
    //  * When inline autocomplete is disabled, we won't be inline
    //    autocompleting this term, so we don't need to worry about confusion as
    //    much.  This also prevents calling Classify() again from inside the
    //    classifier (which will corrupt state and likely crash), since the
    //    classifier always disables inline autocomplete.
    //  * When the user has typed the whole term, the "what you typed" history
    //    match will outrank us for URL-like inputs anyway, so we need not do
    //    anything special.
    if (!prevent_inline_autocomplete && classifier && (i->term != input_text)) {
      AutocompleteMatch match;
      classifier->Classify(i->term, false, false, &match, NULL);
      prevent_inline_autocomplete =
          !AutocompleteMatch::IsSearchType(match.type);
    }

    int relevance = CalculateRelevanceForHistory(i->time, is_keyword,
                                                 prevent_inline_autocomplete);
    scored_results.push_back(SuggestResult(i->term, is_keyword, relevance));
  }

  // History returns results sorted for us.  However, we may have docked some
  // results' scores, so things are no longer in order.  Do a stable sort to get
  // things back in order without otherwise disturbing results with equal
  // scores, then force the scores to be unique, so that the order in which
  // they're shown is deterministic.
  std::stable_sort(scored_results.begin(), scored_results.end(),
                   CompareScoredResults());
  int last_relevance = 0;
  for (SuggestResults::iterator i(scored_results.begin());
       i != scored_results.end(); ++i) {
    if ((i != scored_results.begin()) && (i->relevance() >= last_relevance))
      i->set_relevance(last_relevance - 1);
    last_relevance = i->relevance();
  }

  return scored_results;
}

void SearchProvider::AddSuggestResultsToMap(const SuggestResults& results,
                                            MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i) {
    const bool is_keyword = results[i].from_keyword_provider();
    const string16& input = is_keyword ? keyword_input_.text() : input_.text();
    AddMatchToMap(results[i].suggestion(), input, results[i].relevance(),
                  AutocompleteMatch::SEARCH_SUGGEST, i, is_keyword, map);
  }
}

int SearchProvider::GetVerbatimRelevance() const {
  // Use the suggested verbatim relevance score if it is non-negative (valid),
  // if inline autocomplete isn't prevented (always show verbatim on backspace),
  // and if it won't suppress verbatim, leaving no default provider matches.
  // Otherwise, if the default provider returned no matches and was still able
  // to suppress verbatim, the user would have no search/nav matches and may be
  // left unable to search using their default provider from the omnibox.
  // Check for results on each verbatim calculation, as results from older
  // queries (on previous input) may be trimmed for failing to inline new input.
  if (default_verbatim_relevance_ >= 0 &&
      !input_.prevent_inline_autocomplete() &&
      (default_verbatim_relevance_ > 0 ||
       !default_suggest_results_.empty() ||
       !default_navigation_results_.empty())) {
    return default_verbatim_relevance_;
  }
  return CalculateRelevanceForVerbatim();
}

int SearchProvider::CalculateRelevanceForVerbatim() const {
  if (!providers_.keyword_provider().empty())
    return 250;
  return CalculateRelevanceForVerbatimIgnoringKeywordModeState();
}

int SearchProvider::
    CalculateRelevanceForVerbatimIgnoringKeywordModeState() const {
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return kNonURLVerbatimRelevance;

    case AutocompleteInput::URL:
      return 850;

    default:
      NOTREACHED();
      return 0;
  }
}

int SearchProvider::GetKeywordVerbatimRelevance() const {
  // Use the suggested verbatim relevance score if it is non-negative (valid),
  // if inline autocomplete isn't prevented (always show verbatim on backspace),
  // and if it won't suppress verbatim, leaving no keyword provider matches.
  // Otherwise, if the keyword provider returned no matches and was still able
  // to suppress verbatim, the user would have no search/nav matches and may be
  // left unable to search using their keyword provider from the omnibox.
  // Check for results on each verbatim calculation, as results from older
  // queries (on previous input) may be trimmed for failing to inline new input.
  if (keyword_verbatim_relevance_ >= 0 &&
      !input_.prevent_inline_autocomplete() &&
      (keyword_verbatim_relevance_ > 0 ||
       !keyword_suggest_results_.empty() ||
       !keyword_navigation_results_.empty())) {
    return keyword_verbatim_relevance_;
  }
  return CalculateRelevanceForKeywordVerbatim(
      keyword_input_.type(), keyword_input_.prefer_keyword());
}

// static
int SearchProvider::CalculateRelevanceForKeywordVerbatim(
    AutocompleteInput::Type type,
    bool prefer_keyword) {
  // This function is responsible for scoring verbatim query matches
  // for non-extension keywords.  KeywordProvider::CalculateRelevance()
  // scores verbatim query matches for extension keywords, as well as
  // for keyword matches (i.e., suggestions of a keyword itself, not a
  // suggestion of a query on a keyword search engine).  These two
  // functions are currently in sync, but there's no reason we
  // couldn't decide in the future to score verbatim matches
  // differently for extension and non-extension keywords.  If you
  // make such a change, however, you should update this comment to
  // describe it, so it's clear why the functions diverge.
  if (prefer_keyword)
    return 1500;
  return (type == AutocompleteInput::QUERY) ? 1450 : 1100;
}

int SearchProvider::CalculateRelevanceForHistory(
    const Time& time,
    bool is_keyword,
    bool prevent_inline_autocomplete) const {
  // The relevance of past searches falls off over time. There are two distinct
  // equations used. If the first equation is used (searches to the primary
  // provider that we want to inline autocomplete), the score starts at 1399 and
  // falls to 1300. If the second equation is used the relevance of a search 15
  // minutes ago is discounted 50 points, while the relevance of a search two
  // weeks ago is discounted 450 points.
  double elapsed_time = std::max((Time::Now() - time).InSecondsF(), 0.);
  bool is_primary_provider = is_keyword || !providers_.has_keyword_provider();
  if (is_primary_provider && !prevent_inline_autocomplete) {
    // Searches with the past two days get a different curve.
    const double autocomplete_time = 2 * 24 * 60 * 60;
    if (elapsed_time < autocomplete_time) {
      return (is_keyword ? 1599 : 1399) - static_cast<int>(99 *
          std::pow(elapsed_time / autocomplete_time, 2.5));
    }
    elapsed_time -= autocomplete_time;
  }

  const int score_discount =
      static_cast<int>(6.5 * std::pow(elapsed_time, 0.3));

  // Don't let scores go below 0.  Negative relevance scores are meaningful in
  // a different way.
  int base_score;
  if (is_primary_provider)
    base_score = (input_.type() == AutocompleteInput::URL) ? 750 : 1050;
  else
    base_score = 200;
  return std::max(0, base_score - score_discount);
}

void SearchProvider::AddMatchToMap(const string16& query_string,
                                   const string16& input_text,
                                   int relevance,
                                   AutocompleteMatch::Type type,
                                   int accepted_suggestion,
                                   bool is_keyword,
                                   MatchMap* map) {
  AutocompleteMatch match(this, relevance, false, type);
  std::vector<size_t> content_param_offsets;
  // Bail out now if we don't actually have a valid provider.
  match.keyword = is_keyword ?
      providers_.keyword_provider() : providers_.default_provider();
  const TemplateURL* provider_url = match.GetTemplateURL(profile_, false);
  if (provider_url == NULL)
    return;

  match.contents.assign(query_string);
  // We do intra-string highlighting for suggestions - the suggested segment
  // will be highlighted, e.g. for input_text = "you" the suggestion may be
  // "youtube", so we'll bold the "tube" section: you*tube*.
  if (input_text != query_string) {
    size_t input_position = match.contents.find(input_text);
    if (input_position == string16::npos) {
      // The input text is not a substring of the query string, e.g. input
      // text is "slasdot" and the query string is "slashdot", so we bold the
      // whole thing.
      match.contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::MATCH));
    } else {
      // TODO(beng): ACMatchClassification::MATCH now seems to just mean
      //             "bold" this. Consider modifying the terminology.
      // We don't iterate over the string here annotating all matches because
      // it looks odd to have every occurrence of a substring that may be as
      // short as a single character highlighted in a query suggestion result,
      // e.g. for input text "s" and query string "southwest airlines", it
      // looks odd if both the first and last s are highlighted.
      if (input_position != 0) {
        match.contents_class.push_back(
            ACMatchClassification(0, ACMatchClassification::NONE));
      }
      match.contents_class.push_back(
          ACMatchClassification(input_position, ACMatchClassification::DIM));
      size_t next_fragment_position = input_position + input_text.length();
      if (next_fragment_position < query_string.length()) {
        match.contents_class.push_back(
            ACMatchClassification(next_fragment_position,
                                  ACMatchClassification::NONE));
      }
    }
  } else {
    // Otherwise, we're dealing with the "default search" result which has no
    // completion.
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  if (input_.type() == AutocompleteInput::FORCED_QUERY)
    match.fill_into_edit.assign(ASCIIToUTF16("?"));
  if (is_keyword)
    match.fill_into_edit.append(match.keyword + char16(' '));
  if (!input_.prevent_inline_autocomplete() &&
      StartsWith(query_string, input_text, false)) {
    match.inline_autocomplete_offset =
        match.fill_into_edit.length() + input_text.length();
  }
  match.fill_into_edit.append(query_string);

  const TemplateURLRef& search_url = provider_url->url_ref();
  DCHECK(search_url.SupportsReplacement());
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(query_string));
  match.search_terms_args->original_query = input_text;
  match.search_terms_args->accepted_suggestion = accepted_suggestion;
  // This is the destination URL sans assisted query stats.  This must be set
  // so the AutocompleteController can properly de-dupe; the controller will
  // eventually overwrite it before it reaches the user.
  match.destination_url =
      GURL(search_url.ReplaceSearchTerms(*match.search_terms_args.get()));

  // Search results don't look like URLs.
  match.transition = is_keyword ?
      content::PAGE_TRANSITION_KEYWORD : content::PAGE_TRANSITION_GENERATED;

  // Try to add |match| to |map|.  If a match for |query_string| is already in
  // |map|, replace it if |match| is more relevant.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  const std::pair<MatchMap::iterator, bool> i = map->insert(
      std::pair<string16, AutocompleteMatch>(
          base::i18n::ToLower(query_string), match));
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

AutocompleteMatch SearchProvider::NavigationToMatch(
    const NavigationResult& navigation) {
  const string16& input = navigation.from_keyword_provider() ?
      keyword_input_.text() : input_.text();
  AutocompleteMatch match(this, navigation.relevance(), false,
                          AutocompleteMatch::NAVSUGGEST);
  match.destination_url = navigation.url();

  // First look for the user's input inside the fill_into_edit as it would be
  // without trimming the scheme, so we can find matches at the beginning of the
  // scheme.
  const string16 untrimmed_fill_into_edit(
      AutocompleteInput::FormattedStringWithEquivalentMeaning(navigation.url(),
          StringForURLDisplay(navigation.url(), true, false)));
  const URLPrefix* prefix =
      URLPrefix::BestURLPrefix(untrimmed_fill_into_edit, input);
  size_t match_start = (prefix == NULL) ?
      untrimmed_fill_into_edit.find(input) : prefix->prefix.length();
  size_t inline_autocomplete_offset = (prefix == NULL) ?
      string16::npos : (match_start + input.length());
  bool trim_http = !HasHTTPScheme(input) && (!prefix || (match_start != 0));

  // Preserve the forced query '?' prefix in |match.fill_into_edit|.
  // Otherwise, user edits to a suggestion would show non-Search results.
  if (input_.type() == AutocompleteInput::FORCED_QUERY) {
    match.fill_into_edit = ASCIIToUTF16("?");
    if (inline_autocomplete_offset != string16::npos)
      ++inline_autocomplete_offset;
  }

  const std::string languages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  const net::FormatUrlTypes format_types =
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP);
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(navigation.url(),
          net::FormatUrl(navigation.url(), languages, format_types,
                         net::UnescapeRule::SPACES, NULL, NULL,
                         &inline_autocomplete_offset));
  if (!input_.prevent_inline_autocomplete())
    match.inline_autocomplete_offset = inline_autocomplete_offset;
  DCHECK((match.inline_autocomplete_offset == string16::npos) ||
         (match.inline_autocomplete_offset <= match.fill_into_edit.length()));

  match.contents = net::FormatUrl(navigation.url(), languages,
      format_types, net::UnescapeRule::SPACES, NULL, NULL, &match_start);
  // If the first match in the untrimmed string was inside a scheme that we
  // trimmed, look for a subsequent match.
  if (match_start == string16::npos)
    match_start = match.contents.find(input);
  // Safe if |match_start| is npos; also safe if the input is longer than the
  // remaining contents after |match_start|.
  AutocompleteMatch::ClassifyLocationInString(match_start, input.length(),
      match.contents.length(), ACMatchClassification::URL,
      &match.contents_class);

  match.description = navigation.description();
  AutocompleteMatch::ClassifyMatchInString(input, match.description,
      ACMatchClassification::NONE, &match.description_class);
  return match;
}

bool SearchProvider::DemoteKeywordNavigationMatchesBelowTopQueryMatch() {
  // First, determine the maximum score of any keyword query match (verbatim or
  // query suggestion).
  int max_query_relevance = GetKeywordVerbatimRelevance();
  if (!keyword_suggest_results_.empty()) {
    // CompareScoredResults sorts by descending relevance; so use min_element
    // to get the highest relevance.
    SuggestResults::const_iterator max_suggest_it = std::min_element(
        keyword_suggest_results_.begin(),
        keyword_suggest_results_.end(),
        CompareScoredResults());
    max_query_relevance =
        std::max(max_suggest_it->relevance(), max_query_relevance);
  }
  // If no query is supposed to appear, then navigational matches cannot
  // be demoted past it.  Bail.
  if (max_query_relevance == 0)
    return false;
  // Now we know we can enforce the minimum score constraint even after
  // the navigation matches are demoted.  Proceed to demote the navigation
  // matches to enforce the query-must-come-first constraint.
  // Sort the keyword navigation results by decreasing relevance scores.
  std::stable_sort(keyword_navigation_results_.begin(),
                   keyword_navigation_results_.end(),
                   CompareScoredResults());
  // Cap the relevance score of all results.
  for (NavigationResults::iterator it = keyword_navigation_results_.begin();
       it != keyword_navigation_results_.end(); ++it) {
    if (it->relevance() >= max_query_relevance) {
      if (max_query_relevance > 0)
        --max_query_relevance;
      it->set_relevance(max_query_relevance);
    } else {
      // No later results will need to be capped.
      break;
    }
  }
  return true;
}

void SearchProvider::UpdateDone() {
  // We're done when the timer isn't running, there are no suggest queries
  // pending, and we're not waiting on Instant.
  done_ = (!timer_.IsRunning() && (suggest_results_pending_ == 0) &&
           (instant_finalized_ || !chrome::IsInstantEnabled(profile_)));
}
