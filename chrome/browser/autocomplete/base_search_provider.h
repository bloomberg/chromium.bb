// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class contains common functionality for search-based autocomplete
// providers. Search provider and zero suggest provider both use it for common
// functionality.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteProviderListener;
class GURL;
class Profile;
class SuggestionDeletionHandler;
class TemplateURL;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider,
                           public net::URLFetcherDelegate {
 public:
  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

  // ID used in creating URLFetcher for deleting suggestion results.
  static const int kDeletionURLFetcherID;

  BaseSearchProvider(AutocompleteProviderListener* listener,
                     Profile* profile,
                     AutocompleteProvider::Type type);

  // Returns whether |match| is flagged as a query that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // Returns a simpler AutocompleteMatch suitable for persistence like in
  // ShortcutsDatabase.
  // NOTE: Use with care. Most likely you want the other CreateSearchSuggestion
  // with protected access.
  static AutocompleteMatch CreateSearchSuggestion(
      const base::string16& suggestion,
      AutocompleteMatchType::Type type,
      bool from_keyword_provider,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data);

  // AutocompleteProvider:
  virtual void Stop(bool clear_cached_results) OVERRIDE;
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

  void set_in_app_list() { in_app_list_ = true; }

 protected:
  // The following keys are used to record additional information on matches.

  // We annotate our AutocompleteMatches with whether their relevance scores
  // were server-provided using this key in the |additional_info| field.
  static const char kRelevanceFromServerKey[];

  // Indicates whether the server said a match should be prefetched.
  static const char kShouldPrefetchKey[];

  // Used to store metadata from the server response, which is needed for
  // prefetching.
  static const char kSuggestMetadataKey[];

  // Used to store a deletion request url for server-provided suggestions.
  static const char kDeletionUrlKey[];

  // These are the values for the above keys.
  static const char kTrue[];
  static const char kFalse[];

  virtual ~BaseSearchProvider();

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
           bool relevance_from_server,
           AutocompleteMatchType::Type type,
           const std::string& deletion_url);
    virtual ~Result();

    bool from_keyword_provider() const { return from_keyword_provider_; }

    const base::string16& match_contents() const { return match_contents_; }
    const ACMatchClassifications& match_contents_class() const {
      return match_contents_class_;
    }

    AutocompleteMatchType::Type type() const { return type_; }
    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }

    bool relevance_from_server() const { return relevance_from_server_; }
    void set_relevance_from_server(bool relevance_from_server) {
      relevance_from_server_ = relevance_from_server;
    }

    const std::string& deletion_url() const { return deletion_url_; }

    // Returns the default relevance value for this result (which may
    // be left over from a previous omnibox input) given the current
    // input and whether the current input caused a keyword provider
    // to be active.
    virtual int CalculateRelevance(const AutocompleteInput& input,
                                   bool keyword_provider_requested) const = 0;

   protected:
    // The contents to be displayed and its style info.
    base::string16 match_contents_;
    ACMatchClassifications match_contents_class_;

    // True if the result came from the keyword provider.
    bool from_keyword_provider_;

    AutocompleteMatchType::Type type_;

    // The relevance score.
    int relevance_;

   private:
    // Whether this result's relevance score was fully or partly calculated
    // based on server information, and thus is assumed to be more accurate.
    // This is ultimately used in
    // SearchProvider::ConvertResultsToAutocompleteMatches(), see comments
    // there.
    bool relevance_from_server_;

    // Optional deletion URL provided with suggestions. Fetching this URL
    // should result in some reasonable deletion behaviour on the server,
    // e.g. deleting this term out of a user's server-side search history.
    std::string deletion_url_;
  };

  class SuggestResult : public Result {
   public:
    SuggestResult(const base::string16& suggestion,
                  AutocompleteMatchType::Type type,
                  const base::string16& match_contents,
                  const base::string16& match_contents_prefix,
                  const base::string16& annotation,
                  const base::string16& answer_contents,
                  const base::string16& answer_type,
                  const std::string& suggest_query_params,
                  const std::string& deletion_url,
                  bool from_keyword_provider,
                  int relevance,
                  bool relevance_from_server,
                  bool should_prefetch,
                  const base::string16& input_text);
    virtual ~SuggestResult();

    const base::string16& suggestion() const { return suggestion_; }
    const base::string16& match_contents_prefix() const {
      return match_contents_prefix_;
    }
    const base::string16& annotation() const { return annotation_; }
    const std::string& suggest_query_params() const {
      return suggest_query_params_;
    }

    const base::string16& answer_contents() const { return answer_contents_; }
    const base::string16& answer_type() const { return answer_type_; }

    bool should_prefetch() const { return should_prefetch_; }

    // Fills in |match_contents_class_| to reflect how |match_contents_| should
    // be displayed and bolded against the current |input_text|.  If
    // |allow_bolding_all| is false and |match_contents_class_| would have all
    // of |match_contents_| bolded, do nothing.
    void ClassifyMatchContents(const bool allow_bolding_all,
                               const base::string16& input_text);

    // Result:
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The search terms to be used for this suggestion.
    base::string16 suggestion_;

    // The contents to be displayed as prefix of match contents.
    // Used for postfix suggestions to display a leading ellipsis (or some
    // equivalent character) to indicate omitted text.
    // Only used to pass this information to about:omnibox's "Additional Info".
    base::string16 match_contents_prefix_;

    // Optional annotation for the |match_contents_| for disambiguation.
    // This may be displayed in the autocomplete match contents, but is defined
    // separately to facilitate different formatting.
    base::string16 annotation_;

    // Optional additional parameters to be added to the search URL.
    std::string suggest_query_params_;

    // Optional formatted Answers result.
    base::string16 answer_contents_;

    // Type of optional formatted Answers result.
    base::string16 answer_type_;

    // Should this result be prefetched?
    bool should_prefetch_;
  };

  class NavigationResult : public Result {
   public:
    NavigationResult(const AutocompleteSchemeClassifier& scheme_classifier,
                     const GURL& url,
                     AutocompleteMatchType::Type type,
                     const base::string16& description,
                     const std::string& deletion_url,
                     bool from_keyword_provider,
                     int relevance,
                     bool relevance_from_server,
                     const base::string16& input_text,
                     const std::string& languages);
    virtual ~NavigationResult();

    const GURL& url() const { return url_; }
    const base::string16& description() const { return description_; }
    const base::string16& formatted_url() const { return formatted_url_; }

    // Fills in |match_contents_| and |match_contents_class_| to reflect how
    // the URL should be displayed and bolded against the current |input_text|
    // and user |languages|.  If |allow_bolding_nothing| is false and
    // |match_contents_class_| would result in an entirely unbolded
    // |match_contents_|, do nothing.
    void CalculateAndClassifyMatchContents(const bool allow_bolding_nothing,
                                           const base::string16& input_text,
                                           const std::string& languages);

    // Result:
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The suggested url for navigation.
    GURL url_;

    // The properly formatted ("fixed up") URL string with equivalent meaning
    // to the one in |url_|.
    base::string16 formatted_url_;

    // The suggested navigational result description; generally the site name.
    base::string16 description_;
  };

  typedef std::vector<SuggestResult> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;
  typedef std::pair<base::string16, std::string> MatchKey;
  typedef std::map<MatchKey, AutocompleteMatch> MatchMap;
  typedef ScopedVector<SuggestionDeletionHandler> SuggestionDeletionHandlers;

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

  // Returns an AutocompleteMatch with the given |autocomplete_provider|
  // for the search |suggestion|, which represents a search via |template_url|.
  // If |template_url| is NULL, returns a match with an invalid destination URL.
  //
  // |input| is the original user input. Text in the input is used to highlight
  // portions of the match contents to distinguish locally-typed text from
  // suggested text.
  //
  // |input| is also necessary for various other details, like whether we should
  // allow inline autocompletion and what the transition type should be.
  // |accepted_suggestion| and |omnibox_start_margin| are used to generate
  // Assisted Query Stats.
  // |append_extra_query_params| should be set if |template_url| is the default
  // search engine, so the destination URL will contain any
  // command-line-specified query params.
  // |from_app_list| should be set if the search was made from the app list.
  static AutocompleteMatch CreateSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const SuggestResult& suggestion,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      int accepted_suggestion,
      int omnibox_start_margin,
      bool append_extra_query_params,
      bool from_app_list);

  // Parses JSON response received from the provider, stripping XSSI
  // protection if needed. Returns the parsed data if successful, NULL
  // otherwise.
  static scoped_ptr<base::Value> DeserializeJsonData(std::string json_data);

  // Returns whether the requirements for requesting zero suggest results
  // are met. The requirements are
  // * The user is enrolled in a zero suggest experiment.
  // * The user is not on the NTP.
  // * The suggest request is sent over HTTPS.  This avoids leaking the current
  //   page URL or personal data in unencrypted network traffic.
  // * The user has suggest enabled in their settings and is not in incognito
  //   mode.  (Incognito disables suggest entirely.)
  // * The user's suggest provider is Google.  We might want to allow other
  //   providers to see this data someday, but for now this has only been
  //   implemented for Google.
  static bool ZeroSuggestEnabled(
     const GURL& suggest_url,
     const TemplateURL* template_url,
     metrics::OmniboxEventProto::PageClassification page_classification,
     Profile* profile);

  // Returns whether we can send the URL of the current page in any suggest
  // requests.  Doing this requires that all the following hold:
  // * ZeroSuggestEnabled() is true, so we meet the requirements above.
  // * The current URL is HTTP, or HTTPS with the same domain as the suggest
  //   server.  Non-HTTP[S] URLs (e.g. FTP/file URLs) may contain sensitive
  //   information.  HTTPS URLs may also contain sensitive information, but if
  //   they're on the same domain as the suggest server, then the relevant
  //   entity could have already seen/logged this data.
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
      metrics::OmniboxEventProto::PageClassification page_classification,
      Profile* profile);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // If the |deletion_url| is valid, then set |match.deletable| to true and
  // save the |deletion_url| into the |match|'s additional info under
  // the key |kDeletionUrlKey|.
  void SetDeletionURL(const std::string& deletion_url,
                      AutocompleteMatch* match);

  // Creates an AutocompleteMatch from |result| to search for the query in
  // |result|. Adds the created match to |map|; if such a match
  // already exists, whichever one has lower relevance is eliminated.
  // |metadata| and |accepted_suggestion| are used for generating an
  // AutocompleteMatch.
  // |mark_as_deletable| indicates whether the match should be marked deletable.
  // NOTE: Any result containing a deletion URL is always marked deletable.
  void AddMatchToMap(const SuggestResult& result,
                     const std::string& metadata,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     MatchMap* map);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |is_keyword_result| indicates
  // whether the response was received from the keyword provider.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(const base::Value& root_val,
                           bool is_keyword_result,
                           Results* results);

  // Prefetches any images in Answers results.
  void PrefetchAnswersImages(const base::DictionaryValue* answers_json);

  // Called at the end of ParseSuggestResults to rank the |results|.
  virtual void SortResults(bool is_keyword,
                           const base::ListValue* relevances,
                           Results* results);

  // Optionally, cache the received |json_data| and return true if we want
  // to stop processing results at this point. The |parsed_data| is the parsed
  // version of |json_data| used to determine if we received an empty result.
  virtual bool StoreSuggestionResponse(const std::string& json_data,
                                       const base::Value& parsed_data);

  // Returns the TemplateURL corresponding to the keyword or default
  // provider based on the value of |is_keyword|.
  virtual const TemplateURL* GetTemplateURL(bool is_keyword) const = 0;

  // Returns the AutocompleteInput for keyword provider or default provider
  // based on the value of |is_keyword|.
  virtual const AutocompleteInput GetInput(bool is_keyword) const = 0;

  // Returns a pointer to a Results object, which will hold suggest results.
  virtual Results* GetResultsToFill(bool is_keyword) = 0;

  // Returns whether the destination URL corresponding to the given |result|
  // should contain command-line-specified query params.
  virtual bool ShouldAppendExtraParams(const SuggestResult& result) const = 0;

  // Stops the suggest query.
  // NOTE: This does not update |done_|.  Callers must do so.
  virtual void StopSuggest() = 0;

  // Clears the current results.
  virtual void ClearAllResults() = 0;

  // Returns the relevance to use if it was not explicitly set by the server.
  virtual int GetDefaultResultRelevance() const = 0;

  // Records in UMA whether the deletion request resulted in success.
  virtual void RecordDeletionResult(bool success) = 0;

  // Records UMA statistics about a suggest server response.
  virtual void LogFetchComplete(bool succeeded, bool is_keyword) = 0;

  // Modify provider-specific UMA statistics.
  virtual void ModifyProviderInfo(
      metrics::OmniboxEventProto_ProviderInfo* provider_info) const;

  // Returns whether the |fetcher| is for the keyword provider.
  virtual bool IsKeywordFetcher(const net::URLFetcher* fetcher) const = 0;

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesriable behavior. Updates |done_|.
  virtual void UpdateMatches() = 0;

  AutocompleteProviderListener* listener_;
  Profile* profile_;

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query. This field is set to true only if the suggestion
  // provider has completed and the response contained
  // '"google:fieldtrialtriggered":true'.
  bool field_trial_triggered_;

  // Same as above except that it is maintained across the current Omnibox
  // session.
  bool field_trial_triggered_in_session_;

  // The number of suggest results that haven't yet arrived. If it's greater
  // than 0, it indicates that one of the URLFetchers is still running.
  int suggest_results_pending_;

 private:
  friend class SearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);

  // Removes the deleted |match| from the list of |matches_|.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // This gets called when we have requested a suggestion deletion from the
  // server to handle the results of the deletion. It will be called after the
  // deletion request completes.
  void OnDeletionComplete(bool success,
                          SuggestionDeletionHandler* handler);

  // Each deletion handler in this vector corresponds to an outstanding request
  // that a server delete a personalized suggestion. Making this a ScopedVector
  // causes us to auto-cancel all such requests on shutdown.
  SuggestionDeletionHandlers deletion_handlers_;

  // True if this provider's results are being displayed in the app list. By
  // default this is false, meaning that the results will be shown in the
  // omnibox.
  bool in_app_list_;

  DISALLOW_COPY_AND_ASSIGN(BaseSearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
