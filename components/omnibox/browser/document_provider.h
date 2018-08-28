// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the document autocomplete provider. This experimental
// provider uses an experimental API with keys and endpoint provided at
// developer build-time, so it is feature-flagged off by default.

#ifndef COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteProviderListener;
class AutocompleteProviderClient;
class PrefService;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Autocomplete provider for personalized documents owned or readable by the
// signed-in user. In practice this is a second request in parallel with that
// to the default search provider.
class DocumentProvider : public AutocompleteProvider {
 public:
  // Creates and returns an instance of this provider.
  static DocumentProvider* Create(AutocompleteProviderClient* client,
                                  AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  // Registers a client-side preference to enable document suggestions.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, CheckFeatureBehindFlag);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteNoIncognito);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteClientSettingOff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteServerBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, IsInputLikelyURL);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, ParseDocumentSearchResults);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithIneligibleFlag);
  DocumentProvider(AutocompleteProviderClient* client,
                   AutocompleteProviderListener* listener);

  ~DocumentProvider() override;

  // Using this provider requires the user is signed-in and has Google set as
  // default search engine.
  bool IsDocumentProviderAllowed(
      PrefService* prefs,
      bool is_incognito,
      bool is_authenticated,
      const TemplateURLService* template_url_service);

  // Determines if the input is a URL, or is the start of the user entering one.
  // We avoid queries for these cases for quality and scaling reasons.
  static bool IsInputLikelyURL(const AutocompleteInput& input);

  // Called when loading is complete.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  // The function updates |matches_| with data parsed from |json_data|.
  // The update is not performed if |json_data| is invalid.
  // Returns whether |matches_| changed.
  bool UpdateResults(const std::string& json_data);

  // Callback for when the loader is available with a valid token.
  void OnDocumentSuggestionsLoaderAvailable(
      std::unique_ptr<network::SimpleURLLoader> loader);

  // Parses document search result JSON.
  // Returns true if |matches| was populated with fresh suggestions.
  bool ParseDocumentSearchResults(const base::Value& root_val,
                                  ACMatches* matches);

  // Whether the server has instructed us to backoff for this session (in
  // cases where the corpus is uninteresting).
  bool backoff_for_session_;

  // Client for accessing TemplateUrlService, prefs, etc.
  AutocompleteProviderClient* client_;

  // Listener to notify when results are available.
  AutocompleteProviderListener* listener_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // For callbacks that may be run after destruction. Must be declared last.
  base::WeakPtrFactory<DocumentProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DocumentProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
