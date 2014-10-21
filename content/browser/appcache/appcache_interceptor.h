// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"

namespace content {
class AppCacheRequestHandler;
class AppCacheServiceImpl;

// An interceptor to hijack requests and potentially service them out of
// the appcache.
class CONTENT_EXPORT AppCacheInterceptor
    : public net::URLRequest::Interceptor {
 public:
  // Registers a singleton instance with the net library.
  // Should be called early in the IO thread prior to initiating requests.
  static void EnsureRegistered() {
    CHECK(GetInstance());
  }

  // Must be called to make a request eligible for retrieval from an appcache.
  static void SetExtraRequestInfo(net::URLRequest* request,
                                  AppCacheServiceImpl* service,
                                  int process_id,
                                  int host_id,
                                  ResourceType resource_type);

  // May be called after response headers are complete to retrieve extra
  // info about the response.
  static void GetExtraResponseInfo(net::URLRequest* request,
                                   int64* cache_id,
                                   GURL* manifest_url);

  // Methods to support cross site navigations.
  static void PrepareForCrossSiteTransfer(net::URLRequest* request,
                                          int old_process_id);
  static void CompleteCrossSiteTransfer(net::URLRequest* request,
                                        int new_process_id,
                                        int new_host_id);

  static AppCacheInterceptor* GetInstance();

  // The appcache system employs two different interceptors. The singleton
  // AppCacheInterceptor derives URLRequest::Interceptor and is used
  // to hijack request handling upon receipt of the response or a redirect.
  // A separate URLRequestInterceptor derivative is used to hijack handling
  // at the very start of request processing. The separate handler allows the
  // content lib to order its collection of net::URLRequestInterceptors.
  static scoped_ptr<net::URLRequestInterceptor> CreateStartInterceptor();

 protected:
  // Override from net::URLRequest::Interceptor:
  net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) override;
  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) override;
  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) override;

 private:
  friend struct DefaultSingletonTraits<AppCacheInterceptor>;
  class StartInterceptor;

  AppCacheInterceptor();
  ~AppCacheInterceptor() override;

  static void SetHandler(net::URLRequest* request,
                         AppCacheRequestHandler* handler);
  static AppCacheRequestHandler* GetHandler(net::URLRequest* request);

  DISALLOW_COPY_AND_ASSIGN(AppCacheInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_INTERCEPTOR_H_
