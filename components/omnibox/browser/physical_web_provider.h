// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class HistoryURLProvider;

namespace base {
class ListValue;
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

 private:
  PhysicalWebProvider(AutocompleteProviderClient* client,
                      HistoryURLProvider* history_url_provider);
  ~PhysicalWebProvider() override;

  // Adds a separate match item to |matches_| for each nearby URL in
  // |metadata_list|, up to the maximum number of matches allowed. If the total
  // number of nearby URLs exceeds this limit, one match is used for an overflow
  // item.
  void ConstructMatches(base::ListValue* metadata_list);

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

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
