// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_

#include "base/basictypes.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace prerender {

extern const char kChromeNavigateExtraDataKey[];

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

// The prerender contents of some experiments should never be swapped in
// by pretending to never match on the URL.  This function will return true
// iff this is the case for the experiment_id specified.
bool IsNoSwapInExperiment(uint8 experiment_id);

// The prerender contents of some experiments should behave identical to the
// control group, regardless of the field trial.  This function will return true
// iff this is the case for the experiment_id specified.
bool IsControlGroupExperiment(uint8 experiment_id);

// Called by URLRequestResponseStarted to gather data about Pagespeed headers
// into the Prerender.PagespeedHeader histogram. Public so it can be accessed
// by the unit test.
void GatherPagespeedData(const content::ResourceType resource_type,
                         const GURL& request_url,
                         const net::HttpResponseHeaders* response_headers);

// Static method gathering stats about a URLRequest for which a response has
// just started.
void URLRequestResponseStarted(net::URLRequest* request);

// Report a URL was canceled due to trying to handle an external URL.
void ReportPrerenderExternalURL();

// Report a URL was canceled due to unsupported prerender scheme.
void ReportUnsupportedPrerenderScheme(const GURL& url);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_UTIL_H_
