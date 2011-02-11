// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connect_interceptor.h"

#include "chrome/browser/net/predictor_api.h"
#include "net/base/load_flags.h"

namespace chrome_browser_net {

ConnectInterceptor::ConnectInterceptor() {
  net::URLRequest::RegisterRequestInterceptor(this);
}

ConnectInterceptor::~ConnectInterceptor() {
  net::URLRequest::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* ConnectInterceptor::MaybeIntercept(
    net::URLRequest* request) {
  GURL request_scheme_host(request->url().GetWithEmptyPath());

  // Learn what URLs are likely to be needed during next startup.
  LearnAboutInitialNavigation(request_scheme_host);

  bool redirected_host = false;
  if (request->referrer().empty()) {
    if (request->url() != request->original_url()) {
      // This request was completed with a redirect.
      GURL original_scheme_host(request->original_url().GetWithEmptyPath());
      if (request_scheme_host != original_scheme_host) {
        redirected_host = true;
        // Don't learn from redirects that take path as an argument, but do
        // learn from short-hand typing entries, such as "cnn.com" redirects to
        // "www.cnn.com".  We can't just check for has_path(), as a mere "/"
        // will count as a path, so we check that the path is at most a "/"
        // (1 character long) to decide the redirect is "definitive" and has no
        // significant path.
        // TODO(jar): It may be ok to learn from all redirects, as the adaptive
        // system will not respond until several identical redirects have taken
        // place.  Hence a use of a path (that changes) wouldn't really be
        // learned from anyway.
        if (request->original_url().path().length() <= 1) {
          // TODO(jar): These definite redirects could be learned much faster.
          LearnFromNavigation(original_scheme_host, request_scheme_host);
        }
      }
    }
  } else {
    GURL referring_scheme_host = GURL(request->referrer()).GetWithEmptyPath();
    bool is_subresource = !(request->load_flags() & net::LOAD_MAIN_FRAME);
    // Learn about our referring URL, for use in the future.
    if (is_subresource)
      LearnFromNavigation(referring_scheme_host, request_scheme_host);
    if (referring_scheme_host == request_scheme_host) {
      // We've already made any/all predictions when we navigated to the
      // referring host, so we can bail out here.
      return NULL;
    }
  }

  // Subresources for main frames usually get predicted when we detected the
  // main frame request - way back in RenderViewHost::Navigate.  So only handle
  // predictions now for subresources or for redirected hosts.
  if ((request->load_flags() & net::LOAD_SUB_FRAME) || redirected_host)
    PredictFrameSubresources(request_scheme_host);
  return NULL;
}

net::URLRequestJob* ConnectInterceptor::MaybeInterceptResponse(
    net::URLRequest* request) {
  return NULL;
}

net::URLRequestJob* ConnectInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    const GURL& location) {
  return NULL;
}

}  // namespace chrome_browser_net
