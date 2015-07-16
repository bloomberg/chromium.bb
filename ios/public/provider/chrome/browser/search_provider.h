// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SEARCH_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SEARCH_PROVIDER_H_

#include <string>

#include "base/macros.h"

namespace ios {

// SearchProvider provides accessor for search features not yet componentized.
class SearchProvider {
 public:
  SearchProvider() {}
  virtual ~SearchProvider() {}

  // Returns whether query extraction is enabled.
  virtual bool IsQueryExtractionEnabled() = 0;

  // Returns a string indicating whether InstantExtended is enabled, suitable
  // for adding as a query parameter to the search requests. Returns an empty
  // string otherwise.
  //
  // |for_search| should be set to true for search requests, in which case this
  // returns a non-empty string only if query extraction is enabled.
  virtual std::string InstantExtendedEnabledParam(bool for_search) = 0;

  // Returns a string that will cause the search results page to update
  // incrementally. Currently, Instant Extended passes a different param to
  // search results pages that also has this effect, so by default this function
  // returns the empty string when Instant Extended is enabled. However, when
  // doing instant search result prerendering, we still need to pass this param,
  // as Instant Extended does not cause incremental updates by default for the
  // prerender page. Callers should set |for_prerender| in this case to force
  // the returned string to be non-empty.
  virtual std::string ForceInstantResultsParam(bool for_prerender) = 0;

  // Returns the start-edge margin of the omnibox in pixels.
  virtual int OmniboxStartMargin() = 0;

  // Returns the value to use for replacements of type
  // GOOGLE_IMAGE_SEARCH_SOURCE.
  virtual std::string GoogleImageSearchSource() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

}  // namespace

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SEARCH_PROVIDER_H_
