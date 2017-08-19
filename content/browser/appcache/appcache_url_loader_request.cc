// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"

namespace content {

// static
std::unique_ptr<AppCacheURLLoaderRequest> AppCacheURLLoaderRequest::Create(
    const ResourceRequest& request) {
  return std::unique_ptr<AppCacheURLLoaderRequest>(
      new AppCacheURLLoaderRequest(request));
}

AppCacheURLLoaderRequest::~AppCacheURLLoaderRequest() {}

const GURL& AppCacheURLLoaderRequest::GetURL() const {
  return request_.url;
}

const std::string& AppCacheURLLoaderRequest::GetMethod() const {
  return request_.method;
}

const GURL& AppCacheURLLoaderRequest::GetSiteForCookies() const {
  return request_.site_for_cookies;
}

const GURL AppCacheURLLoaderRequest::GetReferrer() const {
  return request_.referrer;
}

bool AppCacheURLLoaderRequest::IsSuccess() const {
  if (response_.headers)
    return true;
  return false;
}

bool AppCacheURLLoaderRequest::IsCancelled() const {
  return false;
}

bool AppCacheURLLoaderRequest::IsError() const {
  return false;
}

int AppCacheURLLoaderRequest::GetResponseCode() const {
  if (response_.headers)
    return response_.headers->response_code();
  return 0;
}

std::string AppCacheURLLoaderRequest::GetResponseHeaderByName(
    const std::string& name) const {
  std::string header;
  if (response_.headers)
    response_.headers->GetNormalizedHeader(name, &header);
  return header;
}

ResourceRequest* AppCacheURLLoaderRequest::GetResourceRequest() {
  return &request_;
}

AppCacheURLLoaderRequest* AppCacheURLLoaderRequest::AsURLLoaderRequest() {
  return this;
}

AppCacheURLLoaderRequest::AppCacheURLLoaderRequest(
    const ResourceRequest& request)
    : request_(request) {}

}  // namespace content
