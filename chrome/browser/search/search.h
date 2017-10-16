// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

class GURL;
class Profile;
class TemplateURLService;

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

// Returns true if |url| should be rendered in the Instant renderer process.
bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile);

// Returns true if |contents| is rendered inside the Instant process for
// |profile|.
bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile);

// Returns true if the Instant |url| should use process per site.
bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile);

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(Profile* profile);
bool DefaultSearchProviderIsGoogle(
    const TemplateURLService* template_url_service);

// Returns true if |url| corresponds to a New Tab page.
// TODO(treib): This is confusingly named, as it includes URLs that are related
// to an NTP, but aren't an NTP themselves (such as the NTP's service worker).
bool IsNTPURL(const GURL& url, Profile* profile);

// Returns true if the visible entry of |contents| is a New Tab page rendered
// in an Instant process.
bool IsInstantNTP(const content::WebContents* contents);

// Same as IsInstantNTP but uses |nav_entry| to determine the URL for the page
// instead of using the visible entry.
bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* nav_entry);

// Returns true if |url| corresponds to a New Tab page that would get rendered
// in an Instant process.
bool IsInstantNTPURL(const GURL& url, Profile* profile);


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

// Returns the New Tab page URL for the given |profile|.
GURL GetNewTabPageURL(Profile* profile);

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
