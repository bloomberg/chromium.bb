// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_intercept_job_factory.h"

#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job_manager.h"

class GURL;

namespace net {

const char* kHttpScheme = "http";
const char* kHttpsScheme = "https";

HttpInterceptJobFactory::HttpInterceptJobFactory(
    const URLRequestJobFactory* job_factory,
    ProtocolHandler* protocol_handler)
    : job_factory_(job_factory),
      protocol_handler_(protocol_handler) {
}

HttpInterceptJobFactory::~HttpInterceptJobFactory() {}

bool HttpInterceptJobFactory::SetProtocolHandler(
    const std::string& scheme, ProtocolHandler* protocol_handler) {
  NOTREACHED();
  return false;
}

void HttpInterceptJobFactory::AddInterceptor(Interceptor* interceptor) {
  // Interceptor addition is not allowed.
  NOTREACHED();
}

URLRequestJob* HttpInterceptJobFactory::MaybeCreateJobWithInterceptor(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeCreateJobWithInterceptor(request, network_delegate);
}

URLRequestJob* HttpInterceptJobFactory::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  if (scheme == kHttpScheme || scheme == kHttpsScheme)
    return protocol_handler_->MaybeCreateJob(request, network_delegate);
  return job_factory_->MaybeCreateJobWithProtocolHandler(
      scheme, request, network_delegate);
}

URLRequestJob* HttpInterceptJobFactory::MaybeInterceptRedirect(
    const GURL& location,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeInterceptRedirect(
      location, request, network_delegate);
}

URLRequestJob* HttpInterceptJobFactory::MaybeInterceptResponse(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeInterceptResponse(request, network_delegate);
}

bool HttpInterceptJobFactory::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK(CalledOnValidThread());
  if (scheme == kHttpScheme || scheme == kHttpsScheme)
    return true;
  return job_factory_->IsHandledProtocol(scheme);
}

bool HttpInterceptJobFactory::IsHandledURL(const GURL& url) const {
  if (url.scheme() == kHttpScheme || url.scheme() == kHttpsScheme)
    return true;
  return job_factory_->IsHandledURL(url);
}

}  // namespace net
