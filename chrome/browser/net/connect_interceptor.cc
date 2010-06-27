// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connect_interceptor.h"

#include "chrome/browser/net/predictor_api.h"

namespace chrome_browser_net {

ConnectInterceptor::ConnectInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

ConnectInterceptor::~ConnectInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* ConnectInterceptor::MaybeIntercept(URLRequest* request) {
  if (!request->referrer().empty()) {
    // Learn about our referring URL, for use in the future.
    GURL referring_url(GURL(request->referrer()).GetWithEmptyPath());
    // TODO(jar): Only call if we think this was part of a frame load, and not a
    // link navigation.  For now, we'll "learn" that to preconnect when a user
    // actually does a click... which will probably waste space in our referrers
    // table (since it probably won't be that deterministic).
    LearnFromNavigation(referring_url, request->url().GetWithEmptyPath());
  }
  // Now we use previous learning and setup for our subresources.
  if (request->was_fetched_via_proxy())
    return NULL;
  // TODO(jar): Only call if we believe this is a frame, and might have
  // subresources.  We could "guess" by looking at path extensions (such as
  // foo.jpg or goo.gif etc.), but better would be to get this info from webkit
  // and have it add the info to the request (we currently only set the
  // priority, but we could record whether it was a frame).
  PredictFrameSubresources(request->url().GetWithEmptyPath());
  return NULL;
}

}  // namespace chrome_browser_net
