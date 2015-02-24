// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/task_runner.h"
#include "net/proxy/proxy_config.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class NetLog;
class ProxyInfo;
class ProxyService;
}

class PrefService;

namespace data_reduction_proxy {

class DataReductionProxyEventStore;

class DataReductionProxyConfigurator {
 public:
  // Constructs a configurator. |network_task_runner| should be the task runner
  // for running network operations, |net_log| and |event_store| are used to
  // track network and Data Reduction Proxy events respectively, must not be
  // null, and must outlive this instance.
  DataReductionProxyConfigurator(
      scoped_refptr<base::SequencedTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyEventStore* event_store);

  virtual ~DataReductionProxyConfigurator();

  void set_net_log(net::NetLog* net_log) {
    DCHECK(!net_log_);
    net_log_ = net_log;
    DCHECK(net_log_);
  }

  // Constructs a proxy configuration suitable for enabling the Data Reduction
  // proxy.
  virtual void Enable(bool primary_restricted,
                      bool fallback_restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin,
                      const std::string& ssl_origin);

  // Constructs a proxy configuration suitable for disabling the Data Reduction
  // proxy.
  virtual void Disable();

  // Adds a host pattern to bypass. This should follow the same syntax used
  // in net::ProxyBypassRules; that is, a hostname pattern, a hostname suffix
  // pattern, an IP literal, a CIDR block, or the magic string '<local>'.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  virtual void AddHostPatternToBypass(const std::string& pattern);

  // Adds a URL pattern to bypass the proxy. The base implementation strips
  // everything in |pattern| after the first single slash and then treats it
  // as a hostname pattern.
  virtual void AddURLPatternToBypass(const std::string& pattern);

  // Updates the config for use on the IO thread.
  void UpdateProxyConfigOnIOThread(const net::ProxyConfig& config);

  // Returns the current data reduction proxy config, even if it is not the
  // effective configuration used by the proxy service.
  const net::ProxyConfig& GetProxyConfigOnIOThread() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfiguratorTest, TestBypassList);

  // Used for updating the proxy config on the IO thread.
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;

  // Rules for bypassing the Data Reduction Proxy.
  std::vector<std::string> bypass_rules_;

  // The Data Reduction Proxy's configuration. This contains the list of
  // acceptable data reduction proxies and bypass rules. It should be accessed
  // only on the IO thread.
  net::ProxyConfig config_;

  // Used for logging of network- and Data Reduction Proxy-related events.
  net::NetLog* net_log_;
  DataReductionProxyEventStore* data_reduction_proxy_event_store_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigurator);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
