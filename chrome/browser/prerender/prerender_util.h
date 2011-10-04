// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace prerender {

// ID indicating that no experiment is active.
const uint8 kNoExperiment = 0;

// Extracts a urlencoded URL stored in a url= query parameter from a URL
// supplied, if available, and stores it in alias_url.  Returns whether or not
// the operation succeeded (i.e. a valid URL was found).
bool MaybeGetQueryStringBasedAliasURL(const GURL& url, GURL* alias_url);

// Extracts an experiment stored in the query parameter
// lpe= from the URL supplied, and returns it.
// Returns kNoExperiment if no experiment ID is found, or if the ID
// is not an integer in the range 1 to 9.
uint8 GetQueryStringBasedExperiment(const GURL& url);

// Indicates whether the URL provided has a Google domain
bool IsGoogleDomain(const GURL& url);

// Indicates whether the URL provided could be a Google search result page.
bool IsGoogleSearchResultURL(const GURL& url);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_
