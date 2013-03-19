// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/about_protocol_handler.h"

#include "net/url_request/url_request_about_job.h"

namespace chrome_browser_net {

AboutProtocolHandler::AboutProtocolHandler() {
}

net::URLRequestJob* AboutProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new net::URLRequestAboutJob(request, network_delegate);
}

bool AboutProtocolHandler::IsSafeRedirectTarget(const GURL& location) const {
  return false;
}

}  // namespace chrome_browser_net
