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

#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteProviderListener;
class GURL;
class Profile;
class TemplateURL;

namespace base {
class ListValue;
class Value;
}

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider,
                           public net::URLFetcherDelegate {
 public:
  BaseSearchProvider(AutocompleteProviderListener* listener,
                     Profile* profile,
                     AutocompleteProvider::Type type);

  // Returns whether |match| is flagged as a query that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // AutocompleteProvider:
  virtual void Stop(bool clear_cached_results) OVERRIDE;
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

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
           bool relevance_from_server);
    virtual ~Result();

    bool from_keyword_provider() const { return from_keyword_provider_; }

    const base::string16& match_contents() const { return match_contents_; }
    const ACMatchClassifications& match_contents_class() const {
      return match_contents_class_;
    }

    int relevance() const { return relevance_; }
    void set_relevance(int relevance) { relevance_ = relevance; }

    bool relevance_from_server() const { return relevance_from_server_; }
    void set_relevance_from_server(bool relevance_from_server) {
      relevance_from_server_ = relevance_from_server;
    }

    // Returns if this result is inlineable against the current input |input|.
    // Non-inlineable results are stale.
    virtual bool IsInlineable(const base::string16& input) const = 0;

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
    SuggestResult(const base::string16& suggestion,
                  AutocompleteMatchType::Type type,
                  const base::string16& match_contents,
                  const base::string16& match_contents_prefix,
                  const base::string16& annotation,
                  const std::string& suggest_query_params,
                  const std::string& deletion_url,
                  bool from_keyword_provider,
                  int relevance,
                  bool relevance_from_server,
                  bool should_prefetch,
                  const base::string16& input_text);
    virtual ~SuggestResult();

    const base::string16& suggestion() const { return suggestion_; }
    AutocompleteMatchType::Type type() const { return type_; }
    const base::string16& match_contents_prefix() const {
      return match_contents_prefix_;
    }
    const base::string16& annotation() const { return annotation_; }
    const std::string& suggest_query_params() const {
      return suggest_query_params_;
    }
    const std::string& deletion_url() const { return deletion_url_; }
    bool should_prefetch() const { return should_prefetch_; }

    // Fills in |match_contents_class_| to reflect how |match_contents_| should
    // be displayed and bolded against the current |input_text|.  If
    // |allow_bolding_all| is false and |match_contents_class_| would have all
    // of |match_contents_| bolded, do nothing.
    void ClassifyMatchContents(const bool allow_bolding_all,
                               const base::string16& input_text);

    // Result:
    virtual bool IsInlineable(const base::string16& input) const OVERRIDE;
    virtual int CalculateRelevance(
        const AutocompleteInput& input,
        bool keyword_provider_requested) const OVERRIDE;

   private:
    // The search terms to be used for this suggestion.
    base::string16 suggestion_;

    AutocompleteMatchType::Type type_;

    // The contents to be displayed as prefix of match contents.
    // Used for postfix suggestions to display a leading ellipsis (or some
    // equivalent character) to indicate omitted text.
    base::string16 match_contents_prefix_;

    // Optional annotation for the |match_contents_| for disambiguation.
    // This may be displayed in the autocomplete match contents, but is defined
    // separately to facilitate different formatting.
    base::string16 annotation_;

    // Optional additional parameters to be added to the search URL.
    std::string suggest_query_params_;

    // Optional deletion URL provided with suggestions. Fetching this URL
    // should result in some reasonable deletion behaviour on the server,
    // e.g. deleting this term out of a user's server-side search history.
    std::string deletion_url_;

    // Should this result be prefetched?
    bool should_prefetch_;
  };

  class NavigationResult : public Result {
   public:
    // |provider| is necessary to use StringForURLDisplay() in order to
    // compute |formatted_url_|.
    NavigationResult(const AutocompleteProvider& provider,
                     const GURL& url,
                     const base::string16& description,
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
    virtual bool IsInlineable(const base::string16& input) const OVERRIDE;
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
  static AutocompleteMatch CreateSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const SuggestResult& suggestion,
      const TemplateURL* template_url,
      int accepted_suggestion,
      int omnibox_start_margin,
      bool append_extra_query_params);

  // Parses JSON response received from the provider, stripping XSSI
  // protection if needed. Returns the parsed data if successful, NULL
  // otherwise.
  static scoped_ptr<base::Value> DeserializeJsonData(std::string json_data);

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

  // Creates an AutocompleteMatch from |result| to search for the query in
  // |result|. Adds the created match to |map|; if such a match
  // already exists, whichever one has lower relevance is eliminated.
  // |metadata| and |accepted_suggestion| are used for generating an
  // AutocompleteMatch.
  void AddMatchToMap(const SuggestResult& result,
                     const std::string& metadata,
                     int accepted_suggestion,
                     MatchMap* map);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |is_keyword_result| indicates
  // whether the response was received from the keyword provider.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(const base::Value& root_val,
                           bool is_keyword_result,
                           Results* results);

  // Called at the end of ParseSuggestResults to rank the |results|.
  virtual void SortResults(bool is_keyword,
                           const base::ListValue* relevances,
                           Results* results);

  // Returns the TemplateURL for the given |result|.
  virtual const TemplateURL* GetTemplateURL(
      const SuggestResult& result) const = 0;

  // Returns the AutocompleteInput for keyword provider or default provider
  // based on the value of |is_keyword|.
  virtual const AutocompleteInput GetInput(bool is_keyword) const = 0;

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

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query. This field is set to true only if the suggestion
  // provider has completed and the response contained
  // '"google:fieldtrialtriggered":true'.
  bool field_trial_triggered_;

  // Same as above except that it is maintained across the current Omnibox
  // session.
  bool field_trial_triggered_in_session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseSearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
