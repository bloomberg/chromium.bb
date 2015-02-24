// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_

#include <set>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyEventStore;

// Class responsible for determining when a response should or should not cause
// the data reduction proxy to be bypassed, and to what degree. Owned by the
// DataReductionProxyInterceptor.
class DataReductionProxyBypassProtocol
    : public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Constructs a DataReductionProxyBypassProtocol object. |config| and
  // |event_store| must be non-NULL and outlive |this|.
  DataReductionProxyBypassProtocol(DataReductionProxyConfig* config,
                                   DataReductionProxyEventStore* event_store);

  ~DataReductionProxyBypassProtocol() override;

  // Decides whether to mark the data reduction proxy as temporarily bad and
  // put it on the proxy retry map, which is maintained by the ProxyService of
  // the URLRequestContext. Returns true if the request should be retried.
  // Updates the load flags in |request| for some bypass types, e.g.,
  // "block-once". Returns the DataReductionProxyBypassType (if not NULL).
  bool MaybeBypassProxyAndPrepareToRetry(
      net::URLRequest* request,
      DataReductionProxyBypassType* proxy_bypass_type);

  // Returns true if the request method is idempotent. Only idempotent requests
  // are retried on a bypass. Visible as part of the public API for testing.
  static bool IsRequestIdempotent(const net::URLRequest* request);

 private:
  // Override from NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Must outlive |this|.
  DataReductionProxyConfig* config_;

  // Must outlive |this|.
  DataReductionProxyEventStore* event_store_;

  // The set of data reduction proxies through which a response has come back
  // with the data reduction proxy via header since the last network change.
  // This is only used if the client is part of the field trial to relax the
  // bypass logic around missing via headers in non-4xx responses.
  std::set<net::HostPortPair> via_header_producing_proxies_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyBypassProtocol);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
