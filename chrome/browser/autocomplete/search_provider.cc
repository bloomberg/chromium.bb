// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/search_provider.h"

#include <algorithm>
#include <cmath>

#include "base/callback.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_field_trial.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

using base::Time;
using base::TimeDelta;

namespace {

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

};


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
// static
bool SearchProvider::query_suggest_immediately_ = false;

SearchProvider::SearchProvider(ACProviderListener* listener, Profile* profile)
    : AutocompleteProvider(listener, profile, "Search"),
      providers_(TemplateURLServiceFactory::GetForProfile(profile)),
      suggest_results_pending_(0),
      have_suggest_results_(false),
      instant_finalized_(false) {
  // We use GetSuggestNumberOfGroups() as the group ID to mean "not in field
  // trial."  Field trial groups run from 0 to GetSuggestNumberOfGroups() - 1
  // (inclusive).
  int suggest_field_trial_group_number =
      AutocompleteFieldTrial::GetSuggestNumberOfGroups();
  if (AutocompleteFieldTrial::InSuggestFieldTrial()) {
    suggest_field_trial_group_number =
        AutocompleteFieldTrial::GetSuggestGroupNameAsNumber();
  }
  // Add a beacon to the logs that'll allow us to identify later what
  // suggest field trial group a user is in.  Do this by incrementing a
  // bucket in a histogram, where the bucket represents the user's
  // suggest group id.
  UMA_HISTOGRAM_ENUMERATION(
      "Omnibox.SuggestFieldTrialBeacon",
      suggest_field_trial_group_number,
      AutocompleteFieldTrial::GetSuggestNumberOfGroups() + 1);
}

void SearchProvider::FinalizeInstantQuery(const string16& input_text,
                                          const string16& suggest_text) {
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

  default_provider_suggest_text_ = suggest_text;

  string16 adjusted_input_text(input_text);
  AutocompleteInput::RemoveForcedQueryStringIfNecessary(input_.type(),
                                                        &adjusted_input_text);

  const string16 text = adjusted_input_text + suggest_text;
  // Remove any matches that are identical to |text|. We don't use the
  // destination_url for comparison as it varies depending upon the index passed
  // to TemplateURL::ReplaceSearchTerms.
  for (ACMatches::iterator i = matches_.begin(); i != matches_.end();) {
    if (((i->type == AutocompleteMatch::SEARCH_HISTORY) ||
         (i->type == AutocompleteMatch::SEARCH_SUGGEST)) &&
        (i->fill_into_edit == text)) {
      i = matches_.erase(i);
    } else {
      ++i;
    }
  }

  // Add the new instant suggest result. We give it a rank higher than
  // SEARCH_WHAT_YOU_TYPED so that it gets autocompleted.
  int did_not_accept_default_suggestion = default_suggest_results_.empty() ?
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
        TemplateURLRef::NO_SUGGESTION_CHOSEN;
  MatchMap match_map;
  AddMatchToMap(text, adjusted_input_text,
                CalculateRelevanceForWhatYouTyped() + 1,
                AutocompleteMatch::SEARCH_SUGGEST,
                did_not_accept_default_suggestion, false, &match_map);
  DCHECK_EQ(1u, match_map.size());
  matches_.push_back(match_map.begin()->second);

  listener_->OnProviderUpdate(true);
}

void SearchProvider::Start(const AutocompleteInput& input,
                           bool minimal_changes) {
  matches_.clear();

  instant_finalized_ =
      (input.matches_requested() != AutocompleteInput::ALL_MATCHES);

  // Can't return search/suggest results for bogus input or without a profile.
  if (!profile_ || (input.type() == AutocompleteInput::INVALID)) {
    Stop();
    return;
  }

  keyword_input_text_.clear();
  const TemplateURL* keyword_provider =
      KeywordProvider::GetSubstitutingTemplateURLForInput(profile_, input,
                                                          &keyword_input_text_);
  if (keyword_input_text_.empty())
    keyword_provider = NULL;

  TemplateURLService* model = providers_.template_url_service();
  DCHECK(model);
  model->Load();
  const TemplateURL* default_provider = model->GetDefaultSearchProvider();
  if (default_provider && !default_provider->SupportsReplacement())
    default_provider = NULL;

  if (keyword_provider == default_provider)
    default_provider = NULL;  // No use in querying the same provider twice.

  if (!default_provider && !keyword_provider) {
    // No valid providers.
    Stop();
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
    if (done_)
      default_provider_suggest_text_.clear();
    else
      Stop();
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
    Stop();
    return;
  }

  input_ = input;

  DoHistoryQuery(minimal_changes);
  StartOrStopSuggestQuery(minimal_changes);
  ConvertResultsToAutocompleteMatches();
}

SearchProvider::Result::Result(int relevance) : relevance_(relevance) {}
SearchProvider::Result::~Result() {}

SearchProvider::SuggestResult::SuggestResult(const string16& suggestion,
                                             int relevance)
    : Result(relevance),
      suggestion_(suggestion) {
}

SearchProvider::SuggestResult::~SuggestResult() {}

SearchProvider::NavigationResult::NavigationResult(const GURL& url,
                                                   const string16& description,
                                                   int relevance)
    : Result(relevance),
      url_(url),
      description_(description) {
  DCHECK(url_.is_valid());
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
  DCHECK(!done_);
  suggest_results_pending_ = 0;
  time_suggest_request_sent_ = base::TimeTicks::Now();
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  if (default_url && !default_url->suggestions_url().empty()) {
    suggest_results_pending_++;
    default_fetcher_.reset(CreateSuggestFetcher(kDefaultProviderURLFetcherID,
        default_url->suggestions_url_ref(), input_.text()));
  }
  const TemplateURL* keyword_url = providers_.GetKeywordProviderURL();
  if (keyword_url && !keyword_url->suggestions_url().empty()) {
    suggest_results_pending_++;
    keyword_fetcher_.reset(CreateSuggestFetcher(kKeywordProviderURLFetcherID,
        keyword_url->suggestions_url_ref(), keyword_input_text_));
  }

  // Both the above can fail if the providers have been modified or deleted
  // since the query began.
  if (suggest_results_pending_ == 0) {
    UpdateDone();
    // We only need to update the listener if we're actually done.
    if (done_)
      listener_->OnProviderUpdate(false);
  }
}

void SearchProvider::Stop() {
  StopSuggest();
  ClearResults();
  done_ = true;
  default_provider_suggest_text_.clear();
}

void SearchProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!done_);
  suggest_results_pending_--;
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

  bool is_keyword = (source == keyword_fetcher_.get());
  SuggestResults* suggest_results =
      is_keyword ? &keyword_suggest_results_ : &default_suggest_results_;

  std::string histogram_name =
      "Omnibox.SuggestRequest.Failure.GoogleResponseTime";
  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> root_val(deserializer.Deserialize(NULL, NULL));
    const string16& input = is_keyword ? keyword_input_text_ : input_.text();
    have_suggest_results_ = root_val.get() &&
        ParseSuggestResults(root_val.get(), is_keyword, input, suggest_results);
    histogram_name = "Omnibox.SuggestRequest.Success.GoogleResponseTime";
  }

  // Record response time for suggest requests sent to Google.  We care
  // only about the common case: the Google default provider used in
  // non-keyword mode.
  const TemplateURL* default_url = providers_.GetDefaultProviderURL();
  if (!is_keyword && default_url &&
      (default_url->prepopulate_id() == SEARCH_ENGINE_GOOGLE)) {
    UMA_HISTOGRAM_TIMES(histogram_name,
                        base::TimeTicks::Now() - time_suggest_request_sent_);
  }

  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(!suggest_results->empty());
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
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
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
        keyword_input_text_, num_matches, &keyword_history_results_);
  }
}

void SearchProvider::StartOrStopSuggestQuery(bool minimal_changes) {
  // Don't send any queries to the server until some time has elapsed after
  // the last keypress, to avoid flooding the server with requests we are
  // likely to end up throwing away anyway.
  const int kQueryDelayMs = 200;

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
  ClearResults();

  // We can't start a new query if we're only allowed synchronous results.
  if (input_.matches_requested() != AutocompleteInput::ALL_MATCHES)
    return;

  // We'll have at least one pending fetch. Set it to 1 now, but the value is
  // correctly set in Run. As Run isn't invoked immediately we need to set this
  // now, else we won't think we're waiting on results from the server when we
  // really are.
  suggest_results_pending_ = 1;

  // Kick off a timer that will start the URL fetch if it completes before
  // the user types another character.
  int delay = query_suggest_immediately_ ? 0 : kQueryDelayMs;
  timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(delay), this,
               &SearchProvider::Run);
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

  // Next we check the scheme.  If this is UNKNOWN/REQUESTED_URL/URL with a
  // scheme that isn't http/https/ftp, we shouldn't send it.  Sending things
  // like file: and data: is both a waste of time and a disclosure of
  // potentially private, local data.  Other "schemes" may actually be
  // usernames, and we don't want to send passwords.  If the scheme is OK, we
  // still need to check other cases below.  If this is QUERY, then the presence
  // of these schemes means the user explicitly typed one, and thus this is
  // probably a URL that's being entered and happens to currently be invalid --
  // in which case we again want to run our checks below.  Other QUERY cases are
  // less likely to be URLs and thus we assume we're OK.
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
  have_suggest_results_ = false;
}

content::URLFetcher* SearchProvider::CreateSuggestFetcher(
    int id,
    const TemplateURLRef& suggestions_url,
    const string16& text) {
  DCHECK(suggestions_url.SupportsReplacement());
  content::URLFetcher* fetcher = content::URLFetcher::Create(id,
      GURL(suggestions_url.ReplaceSearchTerms(text,
          TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16())),
      content::URLFetcher::GET, this);
  fetcher->SetRequestContext(profile_->GetRequestContext());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->Start();
  return fetcher;
}

bool SearchProvider::ParseSuggestResults(Value* root_val,
                                         bool is_keyword,
                                         const string16& input_text,
                                         SuggestResults* suggest_results) {
  if (!root_val->IsType(Value::TYPE_LIST))
    return false;
  ListValue* root_list = static_cast<ListValue*>(root_val);

  string16 query_str;
  ListValue* result_list = NULL;
  if ((root_list->GetSize() < 2) || !root_list->GetString(0, &query_str) ||
      (query_str != input_text) || !root_list->GetList(1, &result_list))
    return false;

  // 3rd element: Description list.
  ListValue* description_list = NULL;
  if (root_list->GetSize() > 2)
    root_list->GetList(2, &description_list);

  // 4th element: Disregard the query URL list for now.

  // 5th element: Optional key-value pairs from the Suggest server.
  DictionaryValue* dict_val = NULL;
  ListValue* type_list = NULL;
  if (root_list->GetSize() > 4 && root_list->GetDictionary(4, &dict_val)) {
    // Parse Google Suggest specific type extension.
    const std::string kGoogleSuggestType("google:suggesttype");
    dict_val->GetList(kGoogleSuggestType, &type_list);
  }

  // Add the suggestions in reverse order to assist relevance calculation.
  for (size_t i = result_list->GetSize(); i > 0; --i) {
    size_t current_index = i - 1;
    string16 suggestion;
    if (!result_list->GetString(current_index, &suggestion))
      return false;

    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    // See http://crbug.com/56214
    if (!suggestion.length())
      continue;

    std::string type;
    if (type_list && type_list->GetString(current_index, &type) &&
        (type == "NAVIGATION")) {
      string16 description;
      NavigationResults& navigation_results = is_keyword ?
          keyword_navigation_results_ : default_navigation_results_;
      if ((navigation_results.size() < kMaxMatches) && description_list &&
          description_list->GetString(current_index, &description)) {
        // We can't blindly trust the URL coming from the server to be valid.
        GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(suggestion),
                                         std::string()));
        if (url.is_valid()) {
          // Increment the relevance for successive results to preserve order.
          int relevance = CalculateRelevanceForNavigation(is_keyword) +
                          navigation_results.size();
          navigation_results.push_back(
              NavigationResult(url, description, relevance));
        }
      }
    } else {
      // TODO(kochi): Currently we treat a calculator result as a query, but it
      // is better to have better presentation for caluculator results.
      if (suggest_results->size() < kMaxMatches) {
        // Increment the relevance for successive results to preserve order.
        int relevance = CalculateRelevanceForSuggestion(is_keyword) +
                        suggest_results->size();
        suggest_results->push_back(SuggestResult(suggestion, relevance));
      }
    }
  }

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
  // Keyword what you typed results are handled by the KeywordProvider.

  int verbatim_relevance = CalculateRelevanceForWhatYouTyped();
  int did_not_accept_default_suggestion = default_suggest_results_.empty() ?
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
      TemplateURLRef::NO_SUGGESTION_CHOSEN;
  AddMatchToMap(input_.text(), input_.text(), verbatim_relevance,
                AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
                did_not_accept_default_suggestion, false, &map);
  if (!default_provider_suggest_text_.empty()) {
    AddMatchToMap(input_.text() + default_provider_suggest_text_,
                  input_.text(), verbatim_relevance + 1,
                  AutocompleteMatch::SEARCH_SUGGEST,
                  did_not_accept_default_suggestion, false, &map);
  }

  AddHistoryResultsToMap(keyword_history_results_, true,
                         did_not_accept_keyword_suggestion, &map);
  AddHistoryResultsToMap(default_history_results_, false,
                         did_not_accept_default_suggestion, &map);

  AddSuggestResultsToMap(keyword_suggest_results_, true, &map);
  AddSuggestResultsToMap(default_suggest_results_, false, &map);

  // Now add the most relevant matches from the map to |matches_|.
  matches_.clear();
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches_.push_back(i->second);

  AddNavigationResultsToMatches(keyword_navigation_results_, true);
  AddNavigationResultsToMatches(default_navigation_results_, false);

  // Allow an additional match for "what you typed".
  const size_t max_total_matches = kMaxMatches + 1;
  std::partial_sort(matches_.begin(),
      matches_.begin() + std::min(max_total_matches, matches_.size()),
      matches_.end(), &AutocompleteMatch::MoreRelevant);
  if (matches_.size() > max_total_matches)
    matches_.erase(matches_.begin() + max_total_matches, matches_.end());

  UpdateStarredStateOfMatches();
  UpdateDone();
}

void SearchProvider::AddNavigationResultsToMatches(
    const NavigationResults& navigation_results,
    bool is_keyword) {
  if (!navigation_results.empty()) {
    // TODO(kochi): http://b/1170574  We add only one results for navigational
    // suggestions. If we can get more useful information about the score,
    // consider adding more results.
    matches_.push_back(
        NavigationToMatch(navigation_results.front(), is_keyword));
  }
}

void SearchProvider::AddHistoryResultsToMap(const HistoryResults& results,
                                            bool is_keyword,
                                            int did_not_accept_suggestion,
                                            MatchMap* map) {
  if (results.empty())
    return;

  bool prevent_inline_autocomplete =
      (input_.type() == AutocompleteInput::URL) ||
      input_.prevent_inline_autocomplete();
  const string16& input_text(is_keyword ? keyword_input_text_ : input_.text());
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
  AutocompleteClassifier* classifier = profile_->GetAutocompleteClassifier();
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
      classifier->Classify(i->term, string16(), false, false, &match, NULL);
      prevent_inline_autocomplete =
          match.transition == content::PAGE_TRANSITION_TYPED;
    }

    int relevance = CalculateRelevanceForHistory(i->time, is_keyword,
                                                 prevent_inline_autocomplete);
    scored_results.push_back(SuggestResult(i->term, relevance));
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
                                            bool is_keyword,
                                            MatchMap* map) {
  const string16& text = is_keyword ? keyword_input_text_ : input_.text();
  for (size_t i = 0; i < results.size(); ++i) {
    AddMatchToMap(results[i].suggestion(), text, results[i].relevance(),
                  AutocompleteMatch::SEARCH_SUGGEST, i, is_keyword, map);
  }
}

int SearchProvider::CalculateRelevanceForWhatYouTyped() const {
  if (!providers_.keyword_provider().empty())
    return 250;

  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return 1300;

    case AutocompleteInput::REQUESTED_URL:
      return 1150;

    case AutocompleteInput::URL:
      return 850;

    default:
      NOTREACHED();
      return 0;
  }
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
  bool is_primary_provider = providers_.is_primary_provider(is_keyword);
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

int SearchProvider::CalculateRelevanceForSuggestion(bool for_keyword) const {
  return !providers_.is_primary_provider(for_keyword) ? 100 :
      ((input_.type() == AutocompleteInput::URL) ? 300 : 600);
}

int SearchProvider::CalculateRelevanceForNavigation(bool for_keyword) const {
  return providers_.is_primary_provider(for_keyword) ? 800 : 150;
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
  const TemplateURL* provider_url = match.GetTemplateURL(profile_);
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
  size_t search_start = 0;
  if (input_.type() == AutocompleteInput::FORCED_QUERY) {
    match.fill_into_edit.assign(ASCIIToUTF16("?"));
    ++search_start;
  }
  if (is_keyword) {
    match.fill_into_edit.append(match.keyword + char16(' '));
    search_start += match.keyword.length() + 1;
  }
  match.fill_into_edit.append(query_string);
  // Not all suggestions start with the original input.
  if (!input_.prevent_inline_autocomplete() &&
      !match.fill_into_edit.compare(search_start, input_text.length(),
                                   input_text))
    match.inline_autocomplete_offset = search_start + input_text.length();

  const TemplateURLRef& search_url = provider_url->url_ref();
  DCHECK(search_url.SupportsReplacement());
  match.destination_url = GURL(search_url.ReplaceSearchTerms(query_string,
      accepted_suggestion, input_text));

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
    const NavigationResult& navigation,
    bool is_keyword) {
  const string16& input_text = is_keyword ? keyword_input_text_ : input_.text();
  AutocompleteMatch match(this, navigation.relevance(), false,
                          AutocompleteMatch::NAVSUGGEST);
  match.destination_url = navigation.url();
  match.contents =
      StringForURLDisplay(navigation.url(), true, !HasHTTPScheme(input_text));
  AutocompleteMatch::ClassifyMatchInString(input_text, match.contents,
                                           ACMatchClassification::URL,
                                           &match.contents_class);

  match.description = navigation.description();
  AutocompleteMatch::ClassifyMatchInString(input_text, match.description,
                                           ACMatchClassification::NONE,
                                           &match.description_class);

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  if (input_.type() == AutocompleteInput::FORCED_QUERY)
    match.fill_into_edit.assign(ASCIIToUTF16("?"));
  match.fill_into_edit.append(
      AutocompleteInput::FormattedStringWithEquivalentMeaning(navigation.url(),
                                                              match.contents));
  // TODO(pkasting|msw): Inline-autocomplete nav results; see http://b/1112879.

  return match;
}

void SearchProvider::UpdateDone() {
  // We're done when there are no more suggest queries pending (this is set to 1
  // when the timer is started) and we're not waiting on instant.
  done_ = ((suggest_results_pending_ == 0) &&
           (instant_finalized_ || !InstantController::IsEnabled(profile_)));
}
