// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connect_interceptor.h"

#include "chrome/browser/net/predictor_api.h"
#include "net/base/load_flags.h"

namespace chrome_browser_net {

ConnectInterceptor::ConnectInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

ConnectInterceptor::~ConnectInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* ConnectInterceptor::MaybeIntercept(URLRequest* request) {
  // Learn what URLs are likely to be needed during next startup.
  // Pass actual URL, rather than WithEmptyPath, as we often won't need to do
  // the canonicalization.
  LearnAboutInitialNavigation(request->url());

  bool is_subresource = !(request->load_flags() & net::LOAD_MAIN_FRAME);
  if (is_subresource && !request->referrer().empty()) {
    // Learn about our referring URL, for use in the future.
    GURL referring_url(GURL(request->referrer()).GetWithEmptyPath());
    GURL request_url(request->url().GetWithEmptyPath());
    if (referring_url == request_url) {
      // There is nothing to learn about preconnections when the referrer is
      // already the site needed in the request URL.  Similarly, we've already
      // made any/all predictions when we navigated to the referring_url, so we
      // can bail out here. This will also avoid useless boosting of the number
      // of times we navigated to this site, which was already accounted for by
      // the navigation to the referrering_url.
      return NULL;
    }
    LearnFromNavigation(referring_url, request_url);
  }
  bool is_frame = 0 != (request->load_flags() & (net::LOAD_SUB_FRAME |
                                                 net::LOAD_MAIN_FRAME));
  // Now we use previous learning and setup for our subresources.
  if (is_frame && !request->was_fetched_via_proxy())
    PredictFrameSubresources(request->url().GetWithEmptyPath());
  return NULL;
}

}  // namespace chrome_browser_net
