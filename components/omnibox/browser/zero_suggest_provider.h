// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zero-suggest autocomplete provider. This experimental
// provider is invoked when the user focuses in the omnibox prior to editing,
// and generates search query suggestions based on the current URL.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteProviderListener;

namespace base {
class ListValue;
class Value;
}

namespace net {
class URLFetcher;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Autocomplete provider for searches based on the current URL.
//
// The controller will call Start() with |on_focus| set when the user focuses
// the omnibox. After construction, the autocomplete controller repeatedly calls
// Start() with some user input, each time expecting to receive an updated set
// of matches.
//
// TODO(jered): Consider deleting this class and building this functionality
// into SearchProvider after dogfood and after we break the association between
// omnibox text and suggestions.
class ZeroSuggestProvider : public BaseSearchProvider,
                            public net::URLFetcherDelegate {
 public:
  // Creates and returns an instance of this provider.
  static ZeroSuggestProvider* Create(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // Registers a preference used to cache zero suggest results.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  // Sets |field_trial_triggered_| to false.
  void ResetSession() override;

 private:
  ZeroSuggestProvider(AutocompleteProviderClient* client,
                      AutocompleteProviderListener* listener);

  ~ZeroSuggestProvider() override;

  // BaseSearchProvider:
  const TemplateURL* GetTemplateURL(bool is_keyword) const override;
  const AutocompleteInput GetInput(bool is_keyword) const override;
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Optionally, cache the received |json_data| and return true if we want
  // to stop processing results at this point. The |parsed_data| is the parsed
  // version of |json_data| used to determine if we received an empty result.
  bool StoreSuggestionResponse(const std::string& json_data,
                               const base::Value& parsed_data);

  // Adds AutocompleteMatches for each of the suggestions in |results| to
  // |map|.
  void AddSuggestResultsToMap(
      const SearchSuggestionParser::SuggestResults& results,
      MatchMap* map);

  // Returns an AutocompleteMatch for a navigational suggestion |navigation|.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Fetches zero-suggest suggestions by sending a request using |suggest_url|.
  void Run(const GURL& suggest_url);

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

  // Whether we can show zero suggest without sending |current_page_url| to
  // |suggest_url| search provider. Also checks that other conditions for
  // non-contextual zero suggest are satisfied.
  bool ShouldShowNonContextualZeroSuggest(const GURL& suggest_url,
                                          const GURL& current_page_url) const;

  // Checks whether we have a set of zero suggest results cached, and if so
  // populates |matches_| with cached results.
  void MaybeUseCachedSuggestions();

  AutocompleteProviderListener* listener_;

  // The URL for which a suggestion fetch is pending.
  std::string current_query_;

  // The type of page the user is viewing (a search results page doing search
  // term replacement, an arbitrary URL, etc.).
  metrics::OmniboxEventProto::PageClassification current_page_classification_;

  // Copy of OmniboxEditModel::permanent_text_.
  base::string16 permanent_text_;

  // Fetcher used to retrieve results.
  std::unique_ptr<net::URLFetcher> fetcher_;

  // Suggestion for the current URL.
  AutocompleteMatch current_url_match_;

  // Contains suggest and navigation results as well as relevance parsed from
  // the response for the most recent zero suggest input URL.
  SearchSuggestionParser::Results results_;

  // Whether we are currently showing cached zero suggest results.
  bool results_from_cache_;

  history::MostVisitedURLList most_visited_urls_;

  // Whether we are waiting for a most visited visited urls callback to run.
  bool waiting_for_most_visited_urls_request_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ZeroSuggestProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ZeroSuggestProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
