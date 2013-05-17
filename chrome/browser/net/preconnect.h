// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_

#include "chrome/browser/net/url_info.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}

namespace chrome_browser_net {

// Try to preconnect.  Typically motivated by OMNIBOX to reach search service.
// |count| may be used to request more than one connection be established in
// parallel.
void PreconnectOnUIThread(const GURL& url,
                          const GURL& first_party_for_cookies,
                          UrlInfo::ResolutionMotivation motivation,
                          int count,
                          net::URLRequestContextGetter* getter);

// Try to preconnect.  Typically used by predictor when a subresource probably
// needs a connection. |count| may be used to request more than one connection
// be established in parallel.
void PreconnectOnIOThread(const GURL& url,
                          const GURL& first_party_for_cookies,
                          UrlInfo::ResolutionMotivation motivation,
                          int count,
                          net::URLRequestContextGetter* getter);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
