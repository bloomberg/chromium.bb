// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Search autocomplete provider.  This provider is
// responsible for all autocomplete entries that start with "Search <engine>
// for ...", including searching for the current input string, search
// history, and search suggestions.  An instance of it gets created and
// managed by the autocomplete controller.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/instant_types.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class TemplateURLService;

namespace base {
class Value;
}

namespace net {
class URLFetcher;
}

// Autocomplete provider for searches and suggestions from a search engine.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// Initially the provider creates a match that searches for the current input
// text.  It also starts a task to query the Suggest servers.  When that data
// comes back, the provider creates and returns matches for the best
// suggestions.
class SearchProvider : public AutocompleteProvider,
                       public net::URLFetcherDelegate {
 public:
  SearchProvider(AutocompleteProviderListener* listener, Profile* profile);

  // Marks the instant query as done. If |input_text| is non-empty this changes
  // the 'search what you typed' results text to |input_text| +
  // |suggestion.text|. |input_text| is the text the user input into the edit.
  // |input_text| differs from |input_.text()| if the input contained
  // whitespace.
  //
  // This method also marks the search provider as no longer needing to wait for
  // the instant result.
  void FinalizeInstantQuery(const string16& input_text,
                            const InstantSuggestion& suggestion);
  void ClearInstantSuggestion();

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;
  virtual void Stop(bool clear_cached_results) OVERRIDE;

  // Adds search-provider-specific information to omnibox event logs.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;

  // Sets |field_trial_triggered_in_session_| to false.
  virtual void ResetSession() OVERRIDE;

  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SuggestRelevanceExperiment);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInline);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineSchemeSubstring);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineDomainClassify);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);

  virtual ~SearchProvider();

  // Manages the providers (TemplateURLs) used by SearchProvider. Two providers
  // may be used:
  // . The default provider. This corresponds to the user's default search
  //   engine. This is always used, except for the rare case of no default
  //   engine.
  // . The keyword provider. This is used if the user has typed in a keyword.
  class Providers {
   public:
    explicit Providers(TemplateURLService* template_url_service);

    // Returns true if the specified providers match the two providers cached
    // by this class.
    bool equal(const string16& default_provider,
               const string16& keyword_provider) const {
      return (default_provider == default_provider_) &&
          (keyword_provider == keyword_provider_);
    }

    // Resets the cached providers.
    void set(const string16& default_provider,
             const string16& keyword_provider) {
      default_provider_ = default_provider;
      keyword_provider_ = keyword_provider;
    }

    TemplateURLService* template_url_service() { return template_url_service_; }
    const string16& default_provider() const { return default_provider_; }
    const string16& keyword_provider() const { return keyword_provider_; }

    // NOTE: These may return NULL even if the provider members are nonempty!
    const TemplateURL* GetDefaultProviderURL() const;
    const TemplateURL* GetKeywordProviderURL() const;

    // Returns true if there is a valid keyword provider.
    bool has_keyword_provider() const { return !keyword_provider_.empty(); }

   private:
    TemplateURLService* template_url_service_;

    // Cached across the life of a query so we behave consistently even if the
    // user changes their default while the query is running.
    string16 default_provider_;
    string16 keyword_provider_;

    DISALLOW_COPY_AND_ASSIGN(Providers);
  };

  // The Result classes are intermediate representations of AutocompleteMatches,
  // simply containing relevance-ranked search and navigation suggestions.
  // They may be cached to provide some synchronous matches while requests for
  // new suggestions from updated input are in flight.
  // TODO(msw) Extend these classes to generate their corresponding matches and
  //           other requisite data, in order to consolidate and simplify the
  //           highly fragmented SearchProvider logic for each Result type.
  class Result {
   public:
    // Takes whether the result is from the keyword provider and its
    // assigned relevance score.
    explicit Result(bool from_keyword_provider, int relevance);
    virtual ~Result();

    bool from_keyword_provider() const { return from_keyword_provider_; }

    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }

    // Returns the default relevance value for this result (which may
    // be left over from a previous omnibox input) given the current
    // input and whether the current input caused a keyword provider
    // to be active.
    virtual int CalculateRelevance(const AutocompleteInput& input,
                                   bool keyword_provider_requested) const = 0;

   protected:
    // True if the result came from the keyword provider.
    bool from_keyword_provider_;

    // The relevance score.
    int relevance_;
  };

  class SuggestResult : public Result {
   public:
    SuggestResult(const string16& suggestion,
                  bool from_keyword_provider,
                  int relevance);
    virtual ~SuggestResult();

    const string16& suggestion() const { return suggestion_; }

    // Result:
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The search suggestion string.
    string16 suggestion_;
  };

  class NavigationResult : public Result {
   public:
    NavigationResult(const GURL& url,
                     const string16& description,
                     bool from_keyword_provider,
                     int relevance);
    virtual ~NavigationResult();

    const GURL& url() const { return url_; }
    const string16& description() const { return description_; }

    // Result:
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The suggested url for navigation.
    GURL url_;

    // The suggested navigational result description; generally the site name.
    string16 description_;
  };

  typedef std::vector<SuggestResult> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;
  typedef std::vector<history::KeywordSearchTermVisit> HistoryResults;
  typedef std::map<string16, AutocompleteMatch> MatchMap;

  class CompareScoredResults;

  // Called when timer_ expires.
  void Run();

  // Runs the history query, if necessary. The history query is synchronous.
  // This does not update |done_|.
  void DoHistoryQuery(bool minimal_changes);

  // Determines whether an asynchronous subcomponent query should run for the
  // current input.  If so, starts it if necessary; otherwise stops it.
  // NOTE: This function does not update |done_|.  Callers must do so.
  void StartOrStopSuggestQuery(bool minimal_changes);

  // Returns true when the current query can be sent to the Suggest service.
  // This will be false e.g. when Suggest is disabled, the query contains
  // potentially private data, etc.
  bool IsQuerySuitableForSuggest() const;

  // Stops the suggest query.
  // NOTE: This does not update |done_|.  Callers must do so.
  void StopSuggest();

  // Clears the current results.
  void ClearResults();

  // Remove results that cannot inline auto-complete the current input.
  void RemoveStaleResults();
  static void RemoveStaleSuggestResults(SuggestResults* list,
                                        const string16& input);
  void RemoveStaleNavigationResults(NavigationResults* list,
                                    const string16& input);

  // If |default_provider_suggestion_| (which was suggested for
  // |previous_input|) is still applicable given the |current_input|, adjusts it
  // so it can be reused. Otherwise, clears it.
  void AdjustDefaultProviderSuggestion(const string16& previous_input,
                                       const string16& current_input);

  // Apply calculated relevance scores to the current results.
  void ApplyCalculatedRelevance();
  void ApplyCalculatedSuggestRelevance(SuggestResults* list);
  void ApplyCalculatedNavigationRelevance(NavigationResults* list);

  // Starts a new URLFetcher requesting suggest results from |template_url|;
  // callers own the returned URLFetcher, which is NULL for invalid providers.
  net::URLFetcher* CreateSuggestFetcher(int id,
                                        const TemplateURL* template_url,
                                        const AutocompleteInput& input);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists, depending on whether |is_keyword| is true.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(base::Value* root_val, bool is_keyword);

  // Converts the parsed results to a set of AutocompleteMatches, |matches_|.
  void ConvertResultsToAutocompleteMatches();

  // Checks if suggested relevances violate certain expected constraints.
  // See UpdateMatches() for the use and explanation of these constraints.
  bool IsTopMatchScoreTooLow() const;
  bool IsTopMatchHighRankSearchForURL() const;
  bool IsTopMatchNotInlinable() const;

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesriable behavior. Updates |done_|.
  void UpdateMatches();

  // Converts the top navigation result in |navigation_results| to an
  // AutocompleteMatch and adds it to |matches_|. |is_keyword| must be true if
  // the results come from the keyword provider.
  void AddNavigationResultsToMatches(
      const NavigationResults& navigation_results,
      bool is_keyword);

  // Adds a match for each result in |results| to |map|. |is_keyword| indicates
  // whether the results correspond to the keyword provider or default provider.
  void AddHistoryResultsToMap(const HistoryResults& results,
                              bool is_keyword,
                              int did_not_accept_suggestion,
                              MatchMap* map);

  // Calculates relevance scores for all |results|.
  SuggestResults ScoreHistoryResults(const HistoryResults& results,
                                     bool base_prevent_inline_autocomplete,
                                     bool input_multiple_words,
                                     const string16& input_text,
                                     bool is_keyword);

  // Adds matches for |results| to |map|.
  void AddSuggestResultsToMap(const SuggestResults& results, MatchMap* map);

  // Gets the relevance score for the verbatim result; this value may be
  // provided by the suggest server; otherwise it is calculated locally.
  int GetVerbatimRelevance() const;
  // Calculates the relevance score for the verbatim result from the
  // default search engine.  This version takes into account context:
  // i.e., whether the user has entered a keyword-based search or not.
  int CalculateRelevanceForVerbatim() const;
  // Calculates the relevance score for the verbatim result from the default
  // search engine *ignoring* whether the input is a keyword-based search
  // or not.  This function should only be used to determine the minimum
  // relevance score that the best result from this provider should have.
  // For normal use, prefer the above function.
  int CalculateRelevanceForVerbatimIgnoringKeywordModeState() const;
  // Gets the relevance score for the keyword verbatim result; this
  // value may be provided by the suggest server; otherwise it is
  // calculated locally.
  // TODO(mpearson): Refactor so this duplication isn't necesary or
  // restructure so one static function takes all the parameters it needs
  // (rather than looking at internal state).
  int GetKeywordVerbatimRelevance() const;
  // Calculates the relevance score for the keyword verbatim result (if the
  // input matches one of the profile's keyword).
  static int CalculateRelevanceForKeywordVerbatim(AutocompleteInput::Type type,
                                                  bool prefer_keyword);
  // |time| is the time at which this query was last seen.  |is_keyword|
  // indicates whether the results correspond to the keyword provider or default
  // provider. |prevent_inline_autocomplete| is true if we should not inline
  // autocomplete this query.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword,
                                   bool prevent_inline_autocomplete) const;

  // Creates an AutocompleteMatch for "Search <engine> for |query_string|" with
  // the supplied relevance.  Adds this match to |map|; if such a match already
  // exists, whichever one has lower relevance is eliminated.
  void AddMatchToMap(const string16& query_string,
                     const string16& input_text,
                     int relevance,
                     AutocompleteMatch::Type type,
                     int accepted_suggestion,
                     bool is_keyword,
                     MatchMap* map);

  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(const NavigationResult& navigation);

  // Resets the scores of all |keyword_navigation_results_| matches to
  // be below that of the top keyword query match (the verbatim match
  // as expressed by |keyword_verbatim_relevance_| or keyword query
  // suggestions stored in |keyword_suggest_results_|).  May change
  // the order of matches in |keyword_navigation_results_| in the
  // process.  Returns true if the top scoring keyword match is now
  // a query match of some form (not a navigation match) (i.e.,
  // navigation matches were demoted if necessary).  In other words,
  // returns false if there are no keyword query suggestions and
  // the keyword verbatim match is suppressed.
  bool DemoteKeywordNavigationMatchesBelowTopQueryMatch();

  // Updates the value of |done_| from the internal state.
  void UpdateDone();

  // Maintains the TemplateURLs used.
  Providers providers_;

  // The user's input.
  AutocompleteInput input_;

  // Input when searching against the keyword provider.
  AutocompleteInput keyword_input_;

  // Searches in the user's history that begin with the input text.
  HistoryResults keyword_history_results_;
  HistoryResults default_history_results_;

  // Number of suggest results that haven't yet arrived. If greater than 0 it
  // indicates one of the URLFetchers is still running.
  int suggest_results_pending_;

  // A timer to start a query to the suggest server after the user has stopped
  // typing for long enough.
  base::OneShotTimer<SearchProvider> timer_;

  // The time at which we sent a query to the suggest server.
  base::TimeTicks time_suggest_request_sent_;

  // Fetchers used to retrieve results for the keyword and default providers.
  scoped_ptr<net::URLFetcher> keyword_fetcher_;
  scoped_ptr<net::URLFetcher> default_fetcher_;

  // Suggestions returned by the Suggest server for the input text.
  SuggestResults keyword_suggest_results_;
  SuggestResults default_suggest_results_;

  // Navigational suggestions returned by the server.
  NavigationResults keyword_navigation_results_;
  NavigationResults default_navigation_results_;

  // Flags indicating server supplied relevance scores.
  bool has_default_suggested_relevance_;
  bool has_keyword_suggested_relevance_;

  // The server supplied verbatim relevance scores. Negative values
  // indicate that there is no suggested score; a value of 0
  // suppresses the verbatim result.
  int default_verbatim_relevance_;
  int keyword_verbatim_relevance_;

  // Whether suggest_results_ is valid.
  bool have_suggest_results_;

  // Has FinalizeInstantQuery been invoked since the last |Start|?
  bool instant_finalized_;

  // The |suggestion| parameter passed to FinalizeInstantQuery.
  InstantSuggestion default_provider_suggestion_;

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query.  This field is set to false in Start() and may be set
  // to true if either the default provider or keyword provider has completed
  // and their corresponding suggest response contained
  // '"google:fieldtrialtriggered":true'.
  // If the autocomplete query has not returned, this field is set to false.
  bool field_trial_triggered_;

  // Same as above except that it is maintained across the current Omnibox
  // session.
  bool field_trial_triggered_in_session_;

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
