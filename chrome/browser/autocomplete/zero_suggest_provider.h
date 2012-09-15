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

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteInput;
class GURL;
class PrefService;
class TemplateURLService;

namespace base {
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
  // Creates and returns an instance of this provider if the feature is enabled.
  // Returns NULL if not enabled.
  static ZeroSuggestProvider* Create(AutocompleteProviderListener* listener,
                                     Profile* profile);

  static void RegisterUserPrefs(PrefService* user_prefs);

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool /*minimal_changes*/) OVERRIDE;
  virtual void Stop(bool clear_cached_results) OVERRIDE;

  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Initiates a new fetch for the given |url|, limiting suggestions to those
  // matching |user_text|. |user_text| may be non-empty if the user previously
  // interacted with zero-suggest suggestions and then unfocused the omnibox.
  // TODO(jered): Rip out |user_text| once the first match is decoupled from
  // the current typing in the omnibox.
  void StartZeroSuggest(const GURL& url, const string16& user_text);

 private:
  ZeroSuggestProvider(AutocompleteProviderListener* listener,
                      Profile* profile,
                      const std::string& url_prefix);

  virtual ~ZeroSuggestProvider();

  // Update matches given the user has typed |user_text|.
  void UpdateMatches(const string16& user_text);

  // Fetches zero-suggest suggestions for |current_query_|.
  void Run();

  // Parses results from the zero-suggest server and updates results.
  // Returns true if results were updated.
  bool ParseSuggestResults(base::Value* root_val);

  // Converts the parsed results to a set of AutocompleteMatches and adds them
  // to |matches_|.
  void ConvertResultsToAutocompleteMatches();

  // Adds a URL suggestion for the current URL. This should be in the top
  // position so that pressing enter has the effect of reloading the page.
  void AddMatchForCurrentURL();

  // Adds a query suggestion from response position |result_index| with text
  // |result| to |matches_|. Uses |search_provider| to build a search URL for
  // this match.
  void AddMatchForResult(const TemplateURL* search_provider,
                         size_t result_index,
                         const string16& result);

  // Prefix of the URL from which to fetch zero-suggest suggestions.
  const std::string url_prefix_;

  // Used to build default search engine URLs for suggested queries.
  TemplateURLService* template_url_service_;

  // The URL for which a suggestion fetch is pending.
  std::string current_query_;

  // What the user has typed.
  string16 user_text_;

  // Fetcher used to retrieve results.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Suggestions for the most recent query.
  std::vector<string16> results_;

  DISALLOW_COPY_AND_ASSIGN(ZeroSuggestProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_ZERO_SUGGEST_PROVIDER_H_
