// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "net/proxy/proxy_config.h"

namespace net {
class NetLog;
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyEventCreator;

class DataReductionProxyConfigurator {
 public:
  // Constructs a configurator. |net_log| and |event_creator| are used to
  // track network and Data Reduction Proxy events respectively, must not be
  // null, and must outlive this instance.
  DataReductionProxyConfigurator(net::NetLog* net_log,
                                 DataReductionProxyEventCreator* event_creator);

  virtual ~DataReductionProxyConfigurator();

  // Enables data reduction using the proxy servers in |proxies_for_http|.
  // |secure_transport_restricted| indicates that proxies going over secure
  // transports can not be used. |insecure_proxies_restricted| indicates that
  // insecure proxies can not be used.
  // TODO: crbug.com/675764: Pass a vector of DataReductionProxyServer
  // instead of net::ProxyServer.
  virtual void Enable(
      bool secure_transport_restricted,
      bool insecure_proxies_restricted,
      const std::vector<DataReductionProxyServer>& proxies_for_http);

  // Constructs a proxy configuration suitable for disabling the Data Reduction
  // proxy.
  virtual void Disable();

  // Sets the host patterns to bypass.
  //
  // See net::ProxyBypassRules::ParseFromString for the appropriate syntax.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  void SetBypassRules(const std::string& patterns);

  // Returns the current data reduction proxy config, even if it is not the
  // effective configuration used by the proxy service.
  const net::ProxyConfig& GetProxyConfig() const;

  // Constructs a proxy configuration suitable for enabling the Data Reduction
  // proxy. If true, |secure_transport_restricted| indicates that proxies going
  // over secure transports (HTTPS, QUIC) should/can not be used. If true,
  // |insecure_proxies_restricted| indicates that HTTP proxies cannot be used.
  net::ProxyConfig CreateProxyConfig(
      bool secure_transport_restricted,
      bool insecure_proxies_restricted,
      const std::vector<DataReductionProxyServer>& proxies_for_http) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfiguratorTest, TestBypassList);

  // Rules for bypassing the Data Reduction Proxy.
  net::ProxyBypassRules bypass_rules_;

  // The Data Reduction Proxy's configuration. This contains the list of
  // acceptable data reduction proxies and bypass rules. It should be accessed
  // only on the IO thread.
  net::ProxyConfig config_;

  // Used for logging of network- and Data Reduction Proxy-related events.
  net::NetLog* net_log_;
  DataReductionProxyEventCreator* data_reduction_proxy_event_creator_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigurator);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
