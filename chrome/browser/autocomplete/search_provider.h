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
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class SearchProviderTest;
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
  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

  SearchProvider(AutocompleteProviderListener* listener, Profile* profile);

  // Returns an AutocompleteMatch with the given |autocomplete_provider|,
  // |relevance|, and |type|, which represents a search via |template_url| for
  // |query_string|.  If |template_url| is NULL, returns a match with an invalid
  // destination URL.
  //
  // |input_text| is the original user input, which may differ from
  // |query_string|; e.g. the user typed "foo" and got a search suggestion of
  // "food", which we're now marking up.  This is used to highlight portions of
  // the match contents to distinguish locally-typed text from suggested text.
  //
  // |input| and |is_keyword| are necessary for various other details, like
  // whether we should allow inline autocompletion and what the transition type
  // should be.  |accepted_suggestion| and |omnibox_start_margin| are used along
  // with |input_text| to generate Assisted Query Stats.
  // |append_extra_query_params| should be set if |template_url| is the default
  // search engine, so the destination URL will contain any
  // command-line-specified query params.
  static AutocompleteMatch CreateSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const string16& input_text,
      int relevance,
      AutocompleteMatch::Type type,
      bool is_keyword,
      const string16& match_contents,
      const string16& annotation,
      const TemplateURL* template_url,
      const string16& query_string,
      const std::string& suggest_query_params,
      int accepted_suggestion,
      int omnibox_start_margin,
      bool append_extra_query_params);

  // Returns whether the SearchProvider previously flagged |match| as a query
  // that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // Extracts the suggest response metadata which SearchProvider previously
  // stored for |match|.
  static std::string GetSuggestMetadata(const AutocompleteMatch& match);

  // AutocompleteProvider:
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;
  virtual void ResetSession() OVERRIDE;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

  // This URL may be sent with suggest requests; see comments on CanSendURL().
  void set_current_page_url(const GURL& current_page_url) {
    current_page_url_ = current_page_url;
  }

 private:
  // TODO(hfung): Remove ZeroSuggestProvider as a friend class after
  // refactoring common code to a new base class.
  friend class SearchProviderTest;
  friend class ZeroSuggestProvider;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, CanSendURL);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInline);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineDomainClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineSchemeSubstring);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, RemoveStaleResultsTest);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SuggestRelevanceExperiment);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, ClearPrefetchedResults);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, SetPrefetchQuery);

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
    Result(bool from_keyword_provider,
           int relevance,
           bool relevance_from_server);
    virtual ~Result();

    bool from_keyword_provider() const { return from_keyword_provider_; }

    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }

    bool relevance_from_server() const { return relevance_from_server_; }
    void set_relevance_from_server(bool relevance_from_server) {
      relevance_from_server_ = relevance_from_server;
    }

    // Returns if this result is inlineable against the current input |input|.
    // Non-inlineable results are stale.
    virtual bool IsInlineable(const string16& input) const = 0;

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

   private:
    // Whether this result's relevance score was fully or partly calculated
    // based on server information, and thus is assumed to be more accurate.
    // This is ultimately used in
    // SearchProvider::ConvertResultsToAutocompleteMatches(), see comments
    // there.
    bool relevance_from_server_;
  };

  class SuggestResult : public Result {
   public:
    SuggestResult(const string16& suggestion,
                  const string16& match_contents,
                  const string16& annotation,
                  const std::string& suggest_query_params,
                  bool from_keyword_provider,
                  int relevance,
                  bool relevance_from_server,
                  bool should_prefetch);
    virtual ~SuggestResult();

    const string16& suggestion() const { return suggestion_; }
    const string16& match_contents() const { return match_contents_; }
    const string16& annotation() const { return annotation_; }
    const std::string& suggest_query_params() const {
      return suggest_query_params_;
    }
    bool should_prefetch() const { return should_prefetch_; }

    // Result:
    virtual bool IsInlineable(const string16& input) const OVERRIDE;
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The search terms to be used for this suggestion.
    string16 suggestion_;

    // The contents to be displayed in the autocomplete match.
    string16 match_contents_;

    // Optional annotation for the |match_contents_| for disambiguation.
    // This may be displayed in the autocomplete match contents, but is defined
    // separately to facilitate different formatting.
    string16 annotation_;

    // Optional additional parameters to be added to the search URL.
    std::string suggest_query_params_;

    // Should this result be prefetched?
    bool should_prefetch_;
  };

  class NavigationResult : public Result {
   public:
    // |provider| is necessary to use StringForURLDisplay() in order to
    // compute |formatted_url_|.
    NavigationResult(const AutocompleteProvider& provider,
                     const GURL& url,
                     const string16& description,
                     bool from_keyword_provider,
                     int relevance,
                     bool relevance_from_server);
    virtual ~NavigationResult();

    const GURL& url() const { return url_; }
    const string16& description() const { return description_; }
    const string16& formatted_url() const { return formatted_url_; }

    // Result:
    virtual bool IsInlineable(const string16& input) const OVERRIDE;
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The suggested url for navigation.
    GURL url_;

    // The properly formatted ("fixed up") URL string with equivalent meaning
    // to the one in |url_|.
    string16 formatted_url_;

    // The suggested navigational result description; generally the site name.
    string16 description_;
  };

  class CompareScoredResults;

  typedef std::vector<SuggestResult> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;
  typedef std::vector<history::KeywordSearchTermVisit> HistoryResults;
  typedef std::pair<string16, std::string> MatchKey;
  typedef std::map<MatchKey, AutocompleteMatch> MatchMap;

  // A simple structure bundling most of the information (including
  // both SuggestResults and NavigationResults) returned by a call to
  // the suggest server.
  //
  // This has to be declared after the typedefs since it relies on some of them.
  struct Results {
    Results();
    ~Results();

    // Clears |suggest_results| and |navigation_results| and resets
    // |verbatim_relevance| to -1 (implies unset).
    void Clear();

    // Returns whether any of the results (including verbatim) have
    // server-provided scores.
    bool HasServerProvidedScores() const;

    // Query suggestions sorted by relevance score.
    SuggestResults suggest_results;

    // Navigational suggestions sorted by relevance score.
    NavigationResults navigation_results;

    // The server supplied verbatim relevance scores. Negative values
    // indicate that there is no suggested score; a value of 0
    // suppresses the verbatim result.
    int verbatim_relevance;

    // The JSON metadata associated with this server response.
    std::string metadata;

   private:
    DISALLOW_COPY_AND_ASSIGN(Results);
  };

  virtual ~SearchProvider();

  // Removes non-inlineable results until either the top result can inline
  // autocomplete the current input or verbatim outscores the top result.
  static void RemoveStaleResults(const string16& input,
                                 int verbatim_relevance,
                                 SuggestResults* suggest_results,
                                 NavigationResults* navigation_results);

  // Calculates the relevance score for the keyword verbatim result (if the
  // input matches one of the profile's keyword).
  static int CalculateRelevanceForKeywordVerbatim(AutocompleteInput::Type type,
                                                  bool prefer_keyword);

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;
  virtual void Stop(bool clear_cached_results) OVERRIDE;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

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
  void ClearAllResults();

  // Removes stale results for both default and keyword providers.  See comments
  // on RemoveStaleResults().
  void RemoveAllStaleResults();

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

  // Returns an iterator to the first match in |matches_| which might
  // be chosen as default.  If
  // |autocomplete_result_will_reorder_for_default_match| is false,
  // this simply means the first match; otherwise, it means the first
  // match for which the |allowed_to_be_default_match| member is true.
  ACMatches::const_iterator FindTopMatch(
    bool autocomplete_result_will_reorder_for_default_match) const;

  // Checks if suggested relevances violate certain expected constraints.
  // See UpdateMatches() for the use and explanation of these constraints.
  bool IsTopMatchNavigationInKeywordMode(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool IsTopMatchScoreTooLow(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool IsTopMatchSearchWithURLInput(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool HasValidDefaultMatch(
      bool autocomplete_result_will_reorder_for_default_match) const;

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesriable behavior. Updates |done_|.
  void UpdateMatches();

  // Converts an appropriate number of navigation results in
  // |navigation_results| to matches and adds them to |matches|.
  void AddNavigationResultsToMatches(
      const NavigationResults& navigation_results,
      ACMatches* matches);

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
  void AddSuggestResultsToMap(const SuggestResults& results,
                              const std::string& metadata,
                              MatchMap* map);

  // Gets the relevance score for the verbatim result.  This value may be
  // provided by the suggest server or calculated locally; if
  // |relevance_from_server| is non-NULL, it will be set to indicate which of
  // those is true.
  int GetVerbatimRelevance(bool* relevance_from_server) const;

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

  // Gets the relevance score for the keyword verbatim result.
  // |relevance_from_server| is handled as in GetVerbatimRelevance().
  // TODO(mpearson): Refactor so this duplication isn't necesary or
  // restructure so one static function takes all the parameters it needs
  // (rather than looking at internal state).
  int GetKeywordVerbatimRelevance(bool* relevance_from_server) const;

  // |time| is the time at which this query was last seen.  |is_keyword|
  // indicates whether the results correspond to the keyword provider or default
  // provider. |use_aggressive_method| says whether this function can use a
  // method that gives high scores (1200+) rather than one that gives lower
  // scores.  When using the aggressive method, scores may exceed 1300
  // unless |prevent_search_history_inlining| is set.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword,
                                   bool use_aggressive_method,
                                   bool prevent_search_history_inlining) const;

  // Creates an AutocompleteMatch for "Search <engine> for |query_string|" with
  // the supplied relevance.  Adds this match to |map|; if such a match already
  // exists, whichever one has lower relevance is eliminated.
  void AddMatchToMap(const string16& input_text,
                     int relevance,
                     bool relevance_from_server,
                     bool should_prefetch,
                     const std::string& metadata,
                     AutocompleteMatch::Type type,
                     bool is_keyword,
                     const string16& match_contents,
                     const string16& annotation,
                     const string16& query_string,
                     int accepted_suggestion,
                     const std::string& suggest_query_params,
                     MatchMap* map);

  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(const NavigationResult& navigation);

  // Resets the scores of all |keyword_navigation_results_| matches to
  // be below that of the top keyword query match (the verbatim match
  // as expressed by |keyword_verbatim_relevance_| or keyword query
  // suggestions stored in |keyword_suggest_results_|).  If there
  // are no keyword suggestions and keyword verbatim is suppressed,
  // then drops the suggested relevance scores for the navsuggestions
  // and drops the request to suppress verbatim, thereby introducing the
  // keyword verbatim match which will naturally outscore the navsuggestions.
  void DemoteKeywordNavigationMatchesPastTopQuery();

  // Updates the value of |done_| from the internal state.
  void UpdateDone();

  // Returns whether we can send the URL of the current page in any suggest
  // requests.  Doing this requires that all the following hold:
  // * The user has suggest enabled in their settings and is not in incognito
  //   mode.  (Incognito disables suggest entirely.)
  // * The current URL is HTTP, or HTTPS with the same domain as the suggest
  //   server.  Non-HTTP[S] URLs (e.g. FTP/file URLs) may contain sensitive
  //   information.  HTTPS URLs may also contain sensitive information, but if
  //   they're on the same domain as the suggest server, then the relevant
  //   entity could have already seen/logged this data.
  // * The suggest request is sent over HTTPS.  This avoids leaking the current
  //   page URL in world-readable network traffic.
  // * The user's suggest provider is Google.  We might want to allow other
  //   providers to see this data someday, but for now this has only been
  //   implemented for Google.  Also see next bullet.
  // * The user is OK in principle with sending URLs of current pages to their
  //   provider.  Today, there is no explicit setting that controls this, but if
  //   the user has tab sync enabled and tab sync is unencrypted, then they're
  //   already sending this data to Google for sync purposes.  Thus we use this
  //   setting as a proxy for "it's OK to send such data".  In the future,
  //   especially if we want to support suggest providers other than Google, we
  //   may change this to be a standalone setting or part of some explicit
  //   general opt-in.
  static bool CanSendURL(
      const GURL& current_page_url,
      const GURL& suggest_url,
      const TemplateURL* template_url,
      AutocompleteInput::PageClassification page_classification,
      Profile* profile);

  // The amount of time to wait before sending a new suggest request after the
  // previous one.  Non-const because some unittests modify this value.
  static int kMinimumTimeBetweenSuggestQueriesMs;

  // The following keys are used to record additional information on matches.

  // We annotate our AutocompleteMatches with whether their relevance scores
  // were server-provided using this key in the |additional_info| field.
  static const char kRelevanceFromServerKey[];

  // Indicates whether the server said a match should be prefetched.
  static const char kShouldPrefetchKey[];

  // Used to store metadata from the server response, which is needed for
  // prefetching.
  static const char kSuggestMetadataKey[];

  // These are the values for the above keys.
  static const char kTrue[];
  static const char kFalse[];

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

  // Results from the default and keyword search providers.
  Results default_results_;
  Results keyword_results_;

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

  // If true, search history query suggestions will score low enough that
  // they will not be inlined.
  bool prevent_search_history_inlining_;

  GURL current_page_url_;

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
