// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/search/search_model.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class NavigationEntry;
class WebContents;
}

namespace search {

// For reporting Cacheable NTP navigations.
enum CacheableNTPLoad {
  CACHEABLE_NTP_LOAD_FAILED = 0,
  CACHEABLE_NTP_LOAD_SUCCEEDED = 1,
  CACHEABLE_NTP_LOAD_MAX = 2
};

// Returns whether the suggest is enabled for the given |profile|.
bool IsSuggestPrefEnabled(Profile* profile);

// Extracts and returns search terms from |url|.
base::string16 ExtractSearchTermsFromURL(Profile* profile, const GURL& url);

// Returns true if it is okay to extract search terms from |url|. |url| must
// have a secure scheme and must contain the search terms replacement key for
// the default search provider.
bool IsQueryExtractionAllowedForURL(Profile* profile, const GURL& url);

// Returns true if |url| should be rendered in the Instant renderer process.
bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile);

// Returns true if |contents| is rendered inside the Instant process for
// |profile|.
bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile);

// Returns true if the Instant |url| should use process per site.
bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile);

// Returns true if |url| corresponds to a New Tab page (it can be either an
// Instant Extended NTP or a non-extended NTP). A page that matches the search
// or Instant URL of the default search provider but does not have any search
// terms is considered an Instant NTP.
// TODO(treib): This is confusingly named, as it includes URLs that are related
// to an NTP, but aren't an NTP themselves (such as the Instant URL, e.g.
// https://www.google.com/webhp?espv=2).
bool IsNTPURL(const GURL& url, Profile* profile);

// Returns true if the visible entry of |contents| is a New Tab Page rendered
// by Instant.
bool IsInstantNTP(const content::WebContents* contents);

// Same as IsInstantNTP but uses |nav_entry| to determine the URL for the page
// instead of using the visible entry.
bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* nav_entry);

// Returns true if |url| corresponds to a New Tab page that would get rendered
// by Instant.
bool IsInstantNTPURL(const GURL& url, Profile* profile);

// Returns the Instant URL of the default search engine. Returns an empty GURL
// if the engine doesn't have an Instant URL, or if it shouldn't be used (say
// because it doesn't satisfy the requirements for extended mode or if Instant
// is disabled through preferences). Callers must check that the returned URL is
// valid before using it. |force_instant_results| forces a search page to update
// results incrementally even if that is otherwise disabled by google.com
// preferences.
// NOTE: This method expands the default search engine's instant_url template,
// so it shouldn't be called from SearchTermsData or other such code that would
// lead to an infinite recursion.
GURL GetInstantURL(Profile* profile, bool force_instant_results);

// Returns URLs associated with the default search engine for |profile|.
std::vector<GURL> GetSearchURLs(Profile* profile);

// Returns the default search engine base page URL to prefetch search results.
// Returns an empty URL if 'prefetch_results' flag is set to false in field
// trials.
GURL GetSearchResultPrefetchBaseURL(Profile* profile);


// Transforms the input |url| into its "effective URL". |url| must be an
// Instant URL, i.e. ShouldAssignURLToInstantRenderer must return true. The
// returned URL facilitates grouping process-per-site. The |url| is transformed,
// for example, from
//
//   https://www.google.com/search?espv=1&q=tractors
//
// to the privileged URL
//
//   chrome-search://www.google.com/search?espv=1&q=tractors
//
// Notice the scheme change.
//
// If the input is already a privileged URL then that same URL is returned.
//
// If |url| is that of the online NTP, its host is replaced with "remote-ntp".
// This forces the NTP and search results pages to have different SiteIntances,
// and hence different processes.
GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile);

// Rewrites |url| to the actual NTP URL to use if
//   1. |url| is "chrome://newtab",
//   2. InstantExtended is enabled, and
//   3. |browser_context| doesn't correspond to an incognito profile.
bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context);
// Reverses the operation from HandleNewTabURLRewrite.
bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context);

// -----------------------------------------------------
// The following APIs are exposed for use in tests only.
// -----------------------------------------------------

// Returns the Cacheable New Tab Page URL for the given |profile|.
GURL GetNewTabPageURL(Profile* profile);

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
