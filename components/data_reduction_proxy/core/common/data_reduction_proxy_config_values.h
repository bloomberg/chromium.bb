// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_

class GURL;

namespace net {
class HostPortPair;
class ProxyServer;
}

namespace data_reduction_proxy {

struct DataReductionProxyTypeInfo;

class DataReductionProxyConfigValues {
 public:
  virtual ~DataReductionProxyConfigValues() {}

  // Returns true if the data reduction proxy promo may be shown.
  // This is independent of whether the data reduction proxy is allowed.
  // TODO(bengr): maybe tie to whether proxy is allowed.
  virtual bool promo_allowed() const = 0;

  // Returns true if the data reduction proxy should not actually use the
  // proxy if enabled.
  virtual bool holdback() const = 0;

  // Returns true if the data reduction proxy configuration may be used.
  virtual bool allowed() const = 0;

  // Returns true if the fallback proxy may be used.
  virtual bool fallback_allowed() const = 0;

  // Returns true if the alternative data reduction proxy configuration may be
  // used.
  virtual bool alternative_allowed() const = 0;

  // Returns true if the alternative fallback data reduction proxy
  // configuration may be used.
  virtual bool alternative_fallback_allowed() const = 0;

  // Returns true if the proxy server uses an HTTP tunnel to provide HTTPS
  // proxying.
  virtual bool UsingHTTPTunnel(const net::HostPortPair& proxy_server) const = 0;

  // Returns true if the specified |host_port_pair| matches a data reduction
  // proxy. If true, |proxy_info.proxy_servers.first| will contain the name of
  // the proxy that matches. |proxy_info.proxy_servers.second| will contain the
  // name of the data reduction proxy server that would be used if
  // |proxy_info.proxy_server.first| is bypassed, if one exists. In addition,
  // |proxy_info| will note if the proxy was a fallback, an alternative, or a
  // proxy for ssl; these are not mutually exclusive. |proxy_info| can be NULL
  // if the caller isn't interested in its values. Virtual for testing.
  virtual bool IsDataReductionProxy(const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const = 0;

  // Returns the data reduction proxy primary origin.
  virtual const net::ProxyServer& origin() const = 0;

  // Returns the data reduction proxy fallback origin.
  virtual const net::ProxyServer& fallback_origin() const = 0;

  // Returns the alternative data reduction proxy primary origin.
  virtual const net::ProxyServer& alt_origin() const = 0;

  // Returns the alternative data reduction proxy fallback origin.
  virtual const net::ProxyServer& alt_fallback_origin() const = 0;

  // Returns the data reduction proxy ssl origin that is used with the
  // alternative proxy configuration.
  virtual const net::ProxyServer& ssl_origin() const = 0;

  // Returns the URL to check to decide if the secure proxy origin should be
  // used.
  virtual const GURL& secure_proxy_check_url() const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
