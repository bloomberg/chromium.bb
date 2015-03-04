// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_interceptor.h"

namespace data_reduction_proxy {
class DataReductionProxyBypassProtocol;
class DataReductionProxyConfig;
class DataReductionProxyEventStore;
class DataReductionProxyUsageStats;

// Used to intercept responses that contain explicit and implicit signals
// to bypass the Data Reduction Proxy. If the proxy should be bypassed,
// the interceptor returns a new URLRequestHTTPJob that fetches the resource
// without use of the proxy.
class DataReductionProxyInterceptor : public net::URLRequestInterceptor {
 public:
  // Constructs the interceptor. |config|, |stats|, and |event_store| must
  // outlive |this|. |stats| may be NULL.
  DataReductionProxyInterceptor(DataReductionProxyConfig* config,
                                DataReductionProxyUsageStats* stats,
                                DataReductionProxyEventStore* event_store);

  // Destroys the interceptor.
  ~DataReductionProxyInterceptor() override;

  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // Returns a new URLRequestHTTPJob if the redirect indicates that the Data
  // Reduction Proxy should be bypassed. See |MaybeInterceptResponseOrRedirect|
  // for more details.
  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request, net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  // Returns a new URLRequestHTTPJob if the response indicates that the Data
  // Reduction Proxy should be bypassed. See |MaybeInterceptResponseOrRedirect|
  // for more details.
  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  // Returns a new URLRequestHTTPJob if the response or redirect indicates that
  // the data reduction proxy should be bypassed according to the rules in
  // |bypass_protocol_|. Returns NULL otherwise. If a job is returned, the
  // interceptor's URLRequestInterceptingJobFactory will restart the request.
  net::URLRequestJob* MaybeInterceptResponseOrRedirect(
      net::URLRequest* request, net::NetworkDelegate* network_delegate) const;

  // Must outlive |this| if non-NULL.
  DataReductionProxyUsageStats* usage_stats_;

  // Object responsible for identifying cases when a response should cause the
  // data reduction proxy to be bypassed, and for triggering proxy bypasses in
  // these cases.
  scoped_ptr<DataReductionProxyBypassProtocol> bypass_protocol_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyInterceptor);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
