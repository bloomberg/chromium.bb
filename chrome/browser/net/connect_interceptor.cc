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
  bool is_subresource = !(request->load_flags() & net::LOAD_MAIN_FRAME);
  if (is_subresource && !request->referrer().empty()) {
    // Learn about our referring URL, for use in the future.
    GURL referring_url(request->referrer());
    LearnFromNavigation(referring_url.GetWithEmptyPath(),
                        request->url().GetWithEmptyPath());
  }
  bool is_frame = 0 != (request->load_flags() & (net::LOAD_SUB_FRAME |
                                                 net::LOAD_MAIN_FRAME));
  // Now we use previous learning and setup for our subresources.
  if (is_frame && !request->was_fetched_via_proxy())
    PredictFrameSubresources(request->url().GetWithEmptyPath());
  return NULL;
}

}  // namespace chrome_browser_net
