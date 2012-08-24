// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_INTERCEPT_JOB_FACTORY_H_
#define CHROME_BROWSER_NET_HTTP_INTERCEPT_JOB_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace net {

class URLRequest;
class URLRequestJob;

// This class acts as a wrapper for URLRequestJobFactory. It handles HTTP and
// HTTPS jobs using |protocol_handler_|, but forwards all other schemes to the
// old job factory to be handled there.
class HttpInterceptJobFactory : public URLRequestJobFactory {
 public:
  HttpInterceptJobFactory(const URLRequestJobFactory* job_factory,
                          ProtocolHandler* protocol_handler);
  virtual ~HttpInterceptJobFactory();

  // URLRequestJobFactory implementation
  virtual bool SetProtocolHandler(const std::string& scheme,
                                  ProtocolHandler* protocol_handler) OVERRIDE;
  virtual void AddInterceptor(Interceptor* interceptor) OVERRIDE;
  virtual URLRequestJob* MaybeCreateJobWithInterceptor(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE;
  virtual URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const OVERRIDE;
  virtual URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      URLRequest* request,
      NetworkDelegate* network_delegate) const OVERRIDE;
  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE;
  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) const OVERRIDE;

 private:
  const URLRequestJobFactory* job_factory_;
  ProtocolHandler* protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(HttpInterceptJobFactory);
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_HTTP_INTERCEPT_JOB_FACTORY_H_
