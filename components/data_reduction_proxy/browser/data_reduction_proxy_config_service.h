// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/task_runner.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

class PrefService;

namespace net {
class ProxyConfig;
}

namespace data_reduction_proxy {

// A net::ProxyConfigService implementation that applies data reduction proxy
// settings as overrides to the proxy configuration determined by a
// baseline delegate ProxyConfigService.
class DataReductionProxyConfigService
    : public net::ProxyConfigService,
      public net::ProxyConfigService::Observer {
 public:
  // Takes ownership of the passed |base_service|.
  DataReductionProxyConfigService(
      scoped_ptr<net::ProxyConfigService> base_service);
  virtual ~DataReductionProxyConfigService();

  // ProxyConfigService implementation:
  virtual void AddObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE;
  virtual ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) OVERRIDE;
  virtual void OnLazyPoll() OVERRIDE;

  // Method on IO thread that receives the data reduction proxy settings pushed
  // from DataReductionProxyConfiguratorImpl.
  void UpdateProxyConfig(bool enabled,
                         const net::ProxyConfig& config);

 private:
  friend class DataReductionProxyConfigServiceTest;

  // ProxyConfigService::Observer implementation:
  virtual void OnProxyConfigChanged(const net::ProxyConfig& config,
                                    ConfigAvailability availability) OVERRIDE;

  // Makes sure that the observer registration with the base service is set up.
  void RegisterObserver();

  scoped_ptr<net::ProxyConfigService> base_service_;
  ObserverList<net::ProxyConfigService::Observer, true> observers_;

  // Configuration as defined by the data reduction proxy.
  net::ProxyConfig config_;

  // Flag that indicates that a PrefProxyConfigTracker needs to inform us
  // about a proxy configuration before we may return any configuration.
  bool config_read_pending_;

  // Indicates whether the base service registration is done.
  bool registered_observer_;

  // The data reduction proxy is enabled.
  bool enabled_;

  // Use of the data reduction proxy is restricted to HTTP proxying only.
  bool restricted_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigService);
};

// A data_reduction_proxy::DataReductionProxyConfigurator implementation that
// tracks changes to the data reduction proxy configuration and notifies an
// associated DataReductionProxyConfigService. Configuration changes include
// adding URL and host patterns to bypass and enabling and disabling use of the
// proxy.
class DataReductionProxyConfigTracker : public DataReductionProxyConfigurator {
 public:
  DataReductionProxyConfigTracker(
      base::Callback<void(bool, const net::ProxyConfig&)> update_proxy_config,
      base::TaskRunner* task_runner);
  virtual ~DataReductionProxyConfigTracker();

  virtual void Enable(bool primary_restricted,
                      bool fallback_restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin,
                      const std::string& ssl_origin) OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void AddHostPatternToBypass(const std::string& pattern) OVERRIDE;
  virtual void AddURLPatternToBypass(const std::string& pattern) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceTest,
                           TrackerEnable);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceTest,
                           TrackerRestricted);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceTest,
                           TrackerBypassList);

  void UpdateProxyConfigOnIOThread(bool enabled,
                                   const net::ProxyConfig& config);

  base::Callback<void(bool, const net::ProxyConfig&)> update_proxy_config_;
  std::vector<std::string> bypass_rules_;
  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigTracker);
};

}  //  namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_H_
