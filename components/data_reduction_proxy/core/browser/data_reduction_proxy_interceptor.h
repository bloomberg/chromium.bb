// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_

#include "net/url_request/url_request_interceptor.h"

namespace data_reduction_proxy {
class DataReductionProxyParams;
class DataReductionProxyUsageStats;

// Used to intercept responses that contain explicit and implicit signals
// to bypass the data reduction proxy. If the proxy should be bypassed,
// the interceptor returns a new URLRequestHTTPJob that fetches the resource
// without use of the proxy.
class DataReductionProxyInterceptor : public net::URLRequestInterceptor {
 public:
  // Constructs the interceptor. |params| and |stats| must outlive |this|.
  // |stats| may be NULL.
  DataReductionProxyInterceptor(DataReductionProxyParams* params,
                                DataReductionProxyUsageStats* stats);

  // Destroys the interceptor.
  ~DataReductionProxyInterceptor() override;

  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // Returns a new URLRequestHTTPJob if the response indicates that the data
  // reduction proxy should be bypassed according to the rules in
  // data_reduction_proxy_protocol.cc. Returns NULL otherwise. If a job is
  // returned, the interceptor's URLRequestInterceptingJobFactory will restart
  // the request.
  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  // Must outlive |this|.
  DataReductionProxyParams* params_;

  // Must outlive |this| if non-NULL.
  DataReductionProxyUsageStats* usage_stats_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyInterceptor);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
