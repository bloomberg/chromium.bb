// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

namespace base {
class ListValue;
}

class PhysicalWebProvider : public AutocompleteProvider {
public:
  static PhysicalWebProvider* Create(AutocompleteProviderClient* client);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

private:
  PhysicalWebProvider(AutocompleteProviderClient* client);
  ~PhysicalWebProvider() override;

  // Adds a separate match item to |matches_| for each nearby URL in
  // |metadata_list|, up to the maximum number of matches allowed. If the total
  // number of nearby URLs exceeds this limit, one match is used for an overflow
  // item.
  void ConstructMatches(base::ListValue* metadata_list);

  // Adds an overflow match item to |matches_| with a relevance score equal to
  // |relevance| and a label indicating there are |additional_url_count| more
  // nearby URLs. Selecting the overflow item navigates to the Physical Web
  // WebUI, which displays the full list of nearby URLs.
  void AppendOverflowItem(int additional_url_count, int relevance);

  AutocompleteProviderClient* client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_PROVIDER_H_
