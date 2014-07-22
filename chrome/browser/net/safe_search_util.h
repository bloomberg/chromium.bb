// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SAFE_SEARCH_UTIL_H_
#define CHROME_BROWSER_NET_SAFE_SEARCH_UTIL_H_

class GURL;

namespace net {
class HttpRequestHeaders;
class URLRequest;
}

namespace safe_search_util {

// If |request| is a request to Google Web Search, enforces that the SafeSearch
// query parameters are set to active. Sets |new_url| to a copy of the request
// url in which the query part contains the new values of the parameters.
void ForceGoogleSafeSearch(const net::URLRequest* request, GURL* new_url);

// If |request| is a request to YouTube, enforces YouTube's Safety Mode by
// adding/modifying YouTube's PrefCookie header.
void ForceYouTubeSafetyMode(const net::URLRequest* request,
                            net::HttpRequestHeaders* headers);

}  // namespace safe_search_util

#endif  // CHROME_BROWSER_NET_SAFE_SEARCH_UTIL_H_
