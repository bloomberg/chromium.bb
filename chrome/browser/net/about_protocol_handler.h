// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_ABOUT_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_NET_ABOUT_PROTOCOL_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/url_request/url_request_job_factory.h"

namespace chrome_browser_net {

// Implements a ProtocolHandler for About jobs.
class AboutProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  AboutProtocolHandler();
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AboutProtocolHandler);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_ABOUT_PROTOCOL_HANDLER_H_
