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

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider,
                           public net::URLFetcherDelegate {
 public:
  BaseSearchProvider(AutocompleteProviderListener* listener,
                     Profile* profile,
                     AutocompleteProvider::Type type);

 protected:
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

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseSearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
