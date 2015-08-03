// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/search/search_model.h"

class GURL;
class Profile;
class TemplateURL;
class TemplateURLRef;

namespace content {
class BrowserContext;
class NavigationEntry;
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace search {

// For reporting Cacheable NTP navigations.
enum CacheableNTPLoad {
  CACHEABLE_NTP_LOAD_FAILED = 0,
  CACHEABLE_NTP_LOAD_SUCCEEDED = 1,
  CACHEABLE_NTP_LOAD_MAX = 2
};

enum OptInState {
  // The user has not manually opted in/out of InstantExtended.
  INSTANT_EXTENDED_NOT_SET,
  // The user has opted-in to InstantExtended.
  INSTANT_EXTENDED_OPT_IN,
  // The user has opted-out of InstantExtended.
  INSTANT_EXTENDED_OPT_OUT,
  INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT,
};

// Returns whether the suggest is enabled for the given |profile|.
bool IsSuggestPrefEnabled(Profile* profile);

// Extracts and returns search terms from |url|. Does not consider
// IsQueryExtractionEnabled() and Instant support state of the page and does
// not check for a privileged process, so most callers should use
// GetSearchTerms() below instead.
base::string16 ExtractSearchTermsFromURL(Profile* profile, const GURL& url);

// Returns true if it is okay to extract search terms from |url|. |url| must
// have a secure scheme and must contain the search terms replacement key for
// the default search provider.
bool IsQueryExtractionAllowedForURL(Profile* profile, const GURL& url);

// Returns the search terms attached to a specific NavigationEntry, or empty
// string otherwise. Does not consider IsQueryExtractionEnabled() and does not
// check Instant support, so most callers should use GetSearchTerms() below
// instead.
base::string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry);

// Returns search terms if this WebContents is a search results page. It looks
// in the visible NavigationEntry first, to see if search terms have already
// been extracted. Failing that, it tries to extract search terms from the URL.
//
// Returns a blank string if search terms were not found, or if search terms
// extraction is disabled for this WebContents or profile, or if |contents|
// does not support Instant.
base::string16 GetSearchTerms(const content::WebContents* contents);

// Returns true if |url| should be rendered in the Instant renderer process.
bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile);

// Returns true if |contents| is rendered inside the Instant process for
// |profile|.
bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile);

// Returns true if the Instant |url| should use process per site.
bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile);

// Returns true if |url| corresponds to a New Tab page (it can be either an
// Instant Extended NTP or a non-extended NTP).
bool IsNTPURL(const GURL& url, Profile* profile);

// Returns true if the visible entry of |contents| is a New Tab Page rendered
// by Instant. A page that matches the search or Instant URL of the default
// search provider but does not have any search terms is considered an Instant
// New Tab Page.
bool IsInstantNTP(const content::WebContents* contents);

// Same as IsInstantNTP but uses |nav_entry| to determine the URL for the page
// instead of using the visible entry.
bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* nav_entry);

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

// Returns true if 'prerender_instant_url_on_omnibox_focus' flag is enabled in
// field trials to prerender Instant search base page when the omnibox is
// focused.
bool ShouldPrerenderInstantUrlOnOmniboxFocus();

// Returns the Local Instant URL of the New Tab Page.
// TODO(kmadhusu): Remove this function and update the call sites.
GURL GetLocalInstantURL(Profile* profile);

// Returns true if the local new tab page should show a Google logo and search
// box for users whose default search provider is Google, or false if not.
bool ShouldShowGoogleLocalNTP();

// Transforms the input |url| into its "effective URL". The returned URL
// facilitates grouping process-per-site. The |url| is transformed, for
// example, from
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
// If |url| is that of the online NTP, its host is replaced with "online-ntp".
// This forces the NTP and search results pages to have different SiteIntances,
// and hence different processes.
GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile);

// Rewrites |url| if
//   1. |url| is kChromeUINewTabURL,
//   2. InstantExtended is enabled, and
//   3. The --instant-new-tab-url switch is set to a valid URL.
// |url| is rewritten to the value of --instant-new-tab-url.
bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context);
// Reverses the operation from HandleNewTabURLRewrite.
bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context);

// Sets the Instant support |state| in the navigation |entry|.
void SetInstantSupportStateInNavigationEntry(InstantSupportState state,
                                             content::NavigationEntry* entry);

// Returns the Instant support state attached to the NavigationEntry, or
// INSTANT_SUPPORT_UNKNOWN otherwise.
InstantSupportState GetInstantSupportStateFromNavigationEntry(
    const content::NavigationEntry& entry);

// Returns true if the field trial flag is enabled to prefetch results on SRP.
bool ShouldPrefetchSearchResultsOnSRP();

// -----------------------------------------------------
// The following APIs are exposed for use in tests only.
// -----------------------------------------------------

// Returns the Cacheable New Tab Page URL for the given |profile|.
GURL GetNewTabPageURL(Profile* profile);

// Returns true if 'use_alternate_instant_url' flag is set to true in field
// trials to use an alternate Instant search base page URL for prefetching
// search results. This allows experimentation of Instant search.
bool ShouldUseAltInstantURL();

// Returns true if 'use_search_path_for_instant' flag is set to true in field
// trials to use an '/search' path in an alternate Instant search base page URL
// for prefetching search results. This allows experimentation of Instant
// search.
bool ShouldUseSearchPathForInstant();

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
