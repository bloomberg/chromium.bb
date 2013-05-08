// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_JOB_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_JOB_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class URLRequestJobFactoryImpl;
}  // namespace net

namespace android_webview {

// android_webview uses a custom URLRequestJobFactoryImpl to support
// navigation interception and URLRequestJob interception for navigations to
// url with unsupported schemes.
// This is achieved by returning a URLRequestErrorJob for schemes that would
// otherwise be unhandled, which gives the embedder an opportunity to intercept
// the request.
class AwURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  AwURLRequestJobFactory();
  virtual ~AwURLRequestJobFactory();

  bool SetProtocolHandler(const std::string& scheme,
                          ProtocolHandler* protocol_handler);

  // net::URLRequestJobFactory implementation.
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) const OVERRIDE;
  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE;

 private:
  // By default calls are forwarded to this factory, to avoid having to
  // subclass an existing implementation class.
  scoped_ptr<net::URLRequestJobFactoryImpl> next_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestJobFactory);
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_JOB_FACTORY_H_
