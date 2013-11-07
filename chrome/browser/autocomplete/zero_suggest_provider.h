// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zero-suggest autocomplete provider. This experimental
// provider is invoked when the user focuses in the omnibox prior to editing,
// and generates search query suggestions based on the current URL. To enable
// this provider, point --experimental-zero-suggest-url-prefix at an
// appropriate suggestion service.
//
// HUGE DISCLAIMER: This is just here for experimenting and will probably be
// deleted entirely as we revise how suggestions work with the omnibox.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_ZERO_SUGGEST_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_ZERO_SUGGEST_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteInput;
class GURL;
class TemplateURLService;

namespace base {
class ListValue;
class Value;
}

namespace net {
class URLFetcher;
}

// Autocomplete provider for searches based on the current URL.
//
// The controller will call StartZeroSuggest when the user focuses in the
// omnibox. After construction, the autocomplete controller repeatedly calls
// Start() with some user input, each time expecting to receive an updated
// set of matches.
//
// TODO(jered): Consider deleting this class and building this functionality
// into SearchProvider after dogfood and after we break the association between
// omnibox text and suggestions.
class ZeroSuggestProvider : public AutocompleteProvider,
                            public net::URLFetcherDelegate {
 public:
  // Creates and returns an instance of this provider.
  static ZeroSuggestProvider* Create(AutocompleteProviderListener* listener,
                                     Profile* profile);

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool /*minimal_changes*/) OVERRIDE;
  virtual void Stop(bool clear_cached_results) OVERRIDE;

  // Adds provider-specific information to omnibox event logs.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;

  // Sets |field_trial_triggered_| to false.
  virtual void ResetSession() OVERRIDE;

  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Initiates a new fetch for the given |url| of classification
  // |page_classification|. |permanent_text| is the omnibox text
  // for the current page.
  void StartZeroSuggest(
      const GURL& curent_page_url,
      AutocompleteInput::PageClassification page_classification,
      const string16& permanent_text);

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

 private:
  ZeroSuggestProvider(AutocompleteProviderListener* listener,
                      Profile* profile);

  virtual ~ZeroSuggestProvider();

  // The 4 functions below (that take classes defined in SearchProvider as
  // arguments) were copied and trimmed from SearchProvider.
  // TODO(hfung): Refactor them into a new base class common to both
  // ZeroSuggestProvider and SearchProvider.

  // From the OpenSearch formatted response |root_val|, populate query
  // suggestions into |suggest_results|, navigation suggestions into
  // |navigation_results|, and the verbatim relevance score into
  // |verbatim_relevance|.
  void FillResults(const base::Value& root_val,
                   int* verbatim_relevance,
                   SearchProvider::SuggestResults* suggest_results,
                   SearchProvider::NavigationResults* navigation_results);

  // Creates AutocompleteMatches to search |template_url| for "<suggestion>" for
  // all suggestions in |results|, and adds them to |map|.
  void AddSuggestResultsToMap(const SearchProvider::SuggestResults& results,
                              const TemplateURL* template_url,
                              SearchProvider::MatchMap* map);

  // Creates an AutocompleteMatch with the provided |relevance| and |type| to
  // search |template_url| for |query_string|.  |accepted_suggestion| will be
  // used to generate Assisted Query Stats.
  //
  // Adds this match to |map|; if such a match already exists, whichever one
  // has lower relevance is eliminated.
  void AddMatchToMap(int relevance,
                     AutocompleteMatch::Type type,
                     const TemplateURL* template_url,
                     const string16& query_string,
                     int accepted_suggestion,
                     SearchProvider::MatchMap* map);

  // Returns an AutocompleteMatch for a navigational suggestion |navigation|.
  AutocompleteMatch NavigationToMatch(
      const SearchProvider::NavigationResult& navigation);

  // Fetches zero-suggest suggestions by sending a request using |suggest_url|.
  void Run(const GURL& suggest_url);

  // Parses results from the zero-suggest server and updates results.
  void ParseSuggestResults(const base::Value& root_val);

  // Converts the parsed results to a set of AutocompleteMatches and adds them
  // to |matches_|.  Also update the histograms for how many results were
  // received.
  void ConvertResultsToAutocompleteMatches();

  // Returns an AutocompleteMatch for the current URL. The match should be in
  // the top position so that pressing enter has the effect of reloading the
  // page.
  AutocompleteMatch MatchForCurrentURL();

  // When the user is in the Most Visited field trial, we ask the TopSites
  // service for the most visited URLs during Run().  It calls back to this
  // function to return those |urls|.
  void OnMostVisitedUrlsAvailable(const history::MostVisitedURLList& urls);

  // Used to build default search engine URLs for suggested queries.
  TemplateURLService* template_url_service_;

  // The URL for which a suggestion fetch is pending.
  std::string current_query_;

  // The type of page the user is viewing (a search results page doing search
  // term replacement, an arbitrary URL, etc.).
  AutocompleteInput::PageClassification current_page_classification_;

  // Copy of OmniboxEditModel::permanent_text_.
  string16 permanent_text_;

  // Fetcher used to retrieve results.
  scoped_ptr<net::URLFetcher> fetcher_;
  // Whether there's a pending request in flight.
  bool have_pending_request_;

  // Suggestion for the current URL.
  AutocompleteMatch current_url_match_;
  // Navigation suggestions for the most recent ZeroSuggest input URL.
  SearchProvider::NavigationResults navigation_results_;
  // Query suggestions for the most recent ZeroSuggest input URL.
  SearchProvider::MatchMap query_matches_map_;
  // The relevance score for the URL of the current page.
  int verbatim_relevance_;

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query. This field is set to true if the last request
  // was a zero suggest request, the provider has completed and their
  // corresponding response contained '"google:fieldtrialtriggered":true'.
  bool field_trial_triggered_;
  // Whether a zero suggest request triggered a field trial in the omnibox
  // session.  The user could have clicked on a suggestion when zero suggest
  // triggered (same condition as field_trial_triggered_), or triggered zero
  // suggest but kept typing.
  bool field_trial_triggered_in_session_;

  history::MostVisitedURLList most_visited_urls_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ZeroSuggestProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ZeroSuggestProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_ZERO_SUGGEST_PROVIDER_H_
