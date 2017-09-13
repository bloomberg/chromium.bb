// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_

#include "content/browser/appcache/appcache_request.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"

namespace content {

// AppCacheRequest wrapper for the ResourceRequest class for users of
// URLLoader.
class CONTENT_EXPORT AppCacheURLLoaderRequest : public AppCacheRequest {
 public:
  // Factory function to create an instance of the AppCacheResourceRequest
  // class.
  static std::unique_ptr<AppCacheURLLoaderRequest> Create(
      const ResourceRequest& request);

  ~AppCacheURLLoaderRequest() override;

  // AppCacheRequest overrides.
  const GURL& GetURL() const override;
  const std::string& GetMethod() const override;
  const GURL& GetSiteForCookies() const override;
  const GURL GetReferrer() const override;

  bool IsSuccess() const override;
  bool IsCancelled() const override;
  bool IsError() const override;
  int GetResponseCode() const override;
  std::string GetResponseHeaderByName(const std::string& name) const override;

  ResourceRequest* GetResourceRequest() override;
  AppCacheURLLoaderRequest* AsURLLoaderRequest() override;

  void set_request(const ResourceRequest& request) { request_ = request; }
  void set_response(const ResourceResponseHead& response) {
    response_ = response;
  }

 protected:
  explicit AppCacheURLLoaderRequest(const ResourceRequest& request);

 private:
  ResourceRequest request_;
  ResourceResponseHead response_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheURLLoaderRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_
