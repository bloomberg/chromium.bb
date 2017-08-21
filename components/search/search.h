// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_H_
#define COMPONENTS_SEARCH_SEARCH_H_

#include <string>

namespace search {

// Returns whether the Instant Extended API is enabled. This is always true on
// desktop and false on mobile.
bool IsInstantExtendedAPIEnabled();

// Returns a string indicating whether InstantExtended is enabled, suitable
// for adding as a query string param to the homepage or search requests.
std::string InstantExtendedEnabledParam();

// Returns a string that will cause the search results page to update
// incrementally. Currently, Instant Extended passes a different param to
// search results pages that also has this effect, so by default this function
// returns the empty string when Instant Extended is enabled. However, when
// doing instant search result prerendering, we still need to pass this param,
// as Instant Extended does not cause incremental updates by default for the
// prerender page. Callers should set |for_prerender| in this case to force
// the returned string to be non-empty.
std::string ForceInstantResultsParam(bool for_prerender);

}  // namespace search

#endif  // COMPONENTS_SEARCH_SEARCH_H_
