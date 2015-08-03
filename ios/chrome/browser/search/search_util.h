// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_SEARCH_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_SEARCH_UTIL_H_

#include "base/strings/string16.h"

class GURL;

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// Extracts and returns search terms from |url|. Does not consider
// IsQueryExtractionEnabled() and Instant support state of the page and does
// not check for a privileged process, so most callers should use
// GetSearchTerms() below instead.
base::string16 ExtractSearchTermsFromURL(ios::ChromeBrowserState* browser_state,
                                         const GURL& url);

// Returns true if it is okay to extract search terms from |url|. |url| must
// have a secure scheme and must contain the search terms replacement key for
// the default search provider.
bool IsQueryExtractionAllowedForURL(ios::ChromeBrowserState* browser_state,
                                    const GURL& url);

// Returns search terms if this WebState is a search results page. It looks
// in the visible NavigationItem first, to see if search terms have already
// been extracted. Failing that, it tries to extract search terms from the URL.
//
// Returns a blank string if search terms were not found, or if search terms
// extraction is disabled for this WebState or BrowserState, or if |web_state|
// does not support Instant.
base::string16 GetSearchTerms(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_SEARCH_SEARCH_UTIL_H_
