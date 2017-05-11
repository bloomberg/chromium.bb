// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_

#include "content/browser/appcache/appcache_request.h"
#include "content/common/resource_request.h"

namespace content {

// AppCacheRequest wrapper for the ResourceRequest class for users of
// URLLoader.
class CONTENT_EXPORT AppCacheURLLoaderRequest : public AppCacheRequest {
 public:
  // Factory function to create an instance of the AppCacheResourceRequest
  // class.
  static std::unique_ptr<AppCacheURLLoaderRequest> Create(
      std::unique_ptr<ResourceRequest> request);

  ~AppCacheURLLoaderRequest() override;

  // AppCacheRequest overrides.
  // TODO(ananta)
  // Add support to the GetURL() call to return the updated URL while following
  // a chain of redirects.
  const GURL& GetURL() const override;
  const std::string& GetMethod() const override;
  const GURL& GetFirstPartyForCookies() const override;
  const GURL GetReferrer() const override;
  // TODO(ananta)
  // ResourceRequest only identifies the request unlike URLRequest which
  // contains response information as well. We need the following methods to
  // work for AppCache. Look into this.
  bool IsSuccess() const override;
  bool IsCancelled() const override;
  bool IsError() const override;
  int GetResponseCode() const override;
  std::string GetResponseHeaderByName(const std::string& name) const override;

  ResourceRequest* GetResourceRequest() override;

 protected:
  explicit AppCacheURLLoaderRequest(std::unique_ptr<ResourceRequest> request);

 private:
  std::unique_ptr<ResourceRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheURLLoaderRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_REQUEST_H_
