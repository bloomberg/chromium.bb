// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class HistoryURLProvider;

namespace physical_web {
struct Metadata;
using MetadataList = std::vector<Metadata>;
}

class PhysicalWebProvider : public AutocompleteProvider {
 public:
  // The maximum number of match results to provide. If the number of nearby
  // URLs exceeds this limit, an overflow item is created. Tapping the overflow
  // item navigates to a page with the full list of nearby URLs. The overflow
  // item is counted as a match result for the purposes of the match limit.
  //
  // ex: With kPhysicalWebMaxMatches == 1, there should be at most one
  // suggestion  created by this provider. If there is a single nearby URL, then
  // the suggestion will be for that URL. If there are multiple nearby URLs, the
  // suggestion will be the overflow item which navigates to the WebUI when
  // tapped.
  static const size_t kPhysicalWebMaxMatches;

  static PhysicalWebProvider* Create(AutocompleteProviderClient* client,
                                     HistoryURLProvider* history_url_provider);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

 private:
  PhysicalWebProvider(AutocompleteProviderClient* client,
                      HistoryURLProvider* history_url_provider);
  ~PhysicalWebProvider() override;

  // When the user has focused the omnibox but not yet entered any text (i.e.,
  // the Zero Suggest case), calling this method adds a separate match item to
  // |matches_| for each nearby URL in |metadata_list|, up to the maximum number
  // of matches allowed. If the total number of nearby URLs exceeds this limit,
  // one match is used for an overflow item.
  void ConstructZeroSuggestMatches(
      std::unique_ptr<physical_web::MetadataList> metadata_list);

  // When the user has entered text into the omnibox (i.e., the Query Suggest
  // case), calling this method adds a separate match item to |matches_| for
  // each nearby URL in |metadata_list| that matches all of the query terms in
  // |input|, up to the maximum number of matches allowed.
  void ConstructQuerySuggestMatches(
      std::unique_ptr<physical_web::MetadataList> metadata_list,
      const AutocompleteInput& input);

  // Adds an overflow match item to |matches_| with a relevance score equal to
  // |relevance| and a label indicating there are |additional_url_count| more
  // nearby URLs. The page |title| of one of the additional nearby URLs will be
  // included in the label, truncating if necessary. Selecting the overflow item
  // navigates to the Physical Web WebUI, which displays the full list of nearby
  // URLs.
  void AppendOverflowItem(int additional_url_count,
                          int relevance,
                          const base::string16& title);

  AutocompleteProviderClient* client_;

  // Used for efficiency when creating the verbatim match. Can be null.
  HistoryURLProvider* history_url_provider_;

  // The number of nearby Physical Web URLs when the provider last constructed
  // matches. Initialized to string::npos.
  size_t nearby_url_count_;

  // The number of nearby Physical Web URLs when the omnibox input was last
  // focused. Initialized to string::npos.
  // This value is set when the omnibox is focused and recorded when the user
  // selects an omnibox suggestion. If the value is still string::npos when the
  // user makes a selection, it indicates the omnibox was never focused.
  size_t nearby_url_count_at_focus_;

  // If true, provide suggestions when the user has focused the omnibox but has
  // not typed anything.
  bool zero_suggest_enabled_;

  // If true, provide suggestions when the user has entered a query into the
  // omnibox.
  bool after_typing_enabled_;

  // The base relevance score for Physical Web URL suggestions when the user has
  // not typed anything into the omnibox.
  int zero_suggest_base_relevance_;

  // The base relevance score for Physical Web URL suggestions after the user
  // has typed a query into the omnibox.
  int after_typing_base_relevance_;

  // Set to true if at least one suggestion was created the last time the
  // provider was started, even if the suggestion could not be displayed due to
  // a field trial.
  bool had_physical_web_suggestions_;

  // Set to true if at least one suggestion was created since the last time the
  // omnibox was focused, even if the suggestion could not be displayed due to
  // a field trial.
  bool had_physical_web_suggestions_at_focus_or_later_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
