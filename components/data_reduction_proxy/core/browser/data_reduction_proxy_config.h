// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_retry_info.h"

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
}

namespace net {
class HostPortPair;
class NetLog;
class URLRequest;
class URLRequestStatus;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyParams;
class DataReductionProxyService;
struct DataReductionProxyTypeInfo;

// Values of the UMA DataReductionProxy.ProbeURL histogram.
// This enum must remain synchronized with
// DataReductionProxyProbeURLFetchResult in metrics/histograms/histograms.xml.
// TODO(marq): Rename these histogram buckets with s/DISABLED/RESTRICTED/, so
//     their names match the behavior they track.
enum SecureProxyCheckFetchResult {
  // The secure proxy check failed because the Internet was disconnected.
  INTERNET_DISCONNECTED = 0,

  // The secure proxy check failed for any other reason, and as a result, the
  // proxy was disabled.
  FAILED_PROXY_DISABLED,

  // The secure proxy check failed, but the proxy was already restricted.
  FAILED_PROXY_ALREADY_DISABLED,

  // The secure proxy check succeeded, and as a result the proxy was restricted.
  SUCCEEDED_PROXY_ENABLED,

  // The secure proxy check succeeded, but the proxy was already restricted.
  SUCCEEDED_PROXY_ALREADY_ENABLED,

  // This must always be last.
  SECURE_PROXY_CHECK_FETCH_RESULT_COUNT
};

// Central point for holding the Data Reduction Proxy configuration.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyConfig
    : public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime
  // of the |DataReductionProxyConfig| instance, with the exception of |params|
  // which this instance will own.
  DataReductionProxyConfig(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      net::NetLog* net_log,
      scoped_ptr<DataReductionProxyParams> params,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);
  ~DataReductionProxyConfig() override;

  // Returns the underlying |DataReductionProxyParams| instance.
  DataReductionProxyParams* params() const {
    return params_.get();
  }

  void SetDataReductionProxyService(
      base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service);

  // This method expects to run on the UI thread. It permits the Data Reduction
  // Proxy configuration to change based on changes initiated by the user.
  virtual void SetProxyPrefs(bool enabled,
                             bool alternative_enabled,
                             bool at_startup);

  // Returns true if a Data Reduction Proxy was used for the given |request|.
  // If true, |proxy_info.proxy_servers.first| will contain the name of the
  // proxy that was used. |proxy_info.proxy_servers.second| will contain the
  // name of the data reduction proxy server that would be used if
  // |proxy_info.proxy_server.first| is bypassed, if one exists. In addition,
  // |proxy_info| will note if the proxy used was a fallback, an alternative,
  // or a proxy for ssl; these are not mutually exclusive. |proxy_info| can be
  // NULL if the caller isn't interested in its values.
  virtual bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if the specified |host_port_pair| matches a Data Reduction
  // Proxy. If true, |proxy_info.proxy_servers.first| will contain the name of
  // the proxy that matches. |proxy_info.proxy_servers.second| will contain the
  // name of the data reduction proxy server that would be used if
  // |proxy_info.proxy_server.first| is bypassed, if one exists. In addition,
  // |proxy_info| will note if the proxy was a fallback, an alternative, or a
  // proxy for ssl; these are not mutually exclusive. |proxy_info| can be NULL
  // if the caller isn't interested in its values. Virtual for testing.
  virtual bool IsDataReductionProxy(
      const net::HostPortPair& host_port_pair,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if this request would be bypassed by the Data Reduction Proxy
  // based on applying the |data_reduction_proxy_config| param rules to the
  // request URL.
  bool IsBypassedByDataReductionProxyLocalRules(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config) const;

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies that apply to the request scheme. If all possible data reduction
  // proxies are bypassed, returns the minimum retry delay of the bypassed data
  // reduction proxies in min_retry_delay (if not NULL). If there are no
  // bypassed data reduction proxies for the request scheme, returns false and
  // does not assign min_retry_delay.
  bool AreDataReductionProxiesBypassed(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config,
      base::TimeDelta* min_retry_delay) const;

  // Returns true if the proxy is on the retry map and the retry delay is not
  // expired. If proxy is bypassed, retry_delay (if not NULL) returns the delay
  // of proxy_server. If proxy is not bypassed, retry_delay is not assigned.
  bool IsProxyBypassed(const net::ProxyRetryInfoMap& retry_map,
                       const net::ProxyServer& proxy_server,
                       base::TimeDelta* retry_delay) const;

  // Check whether the |proxy_rules| contain any of the data reduction proxies.
  virtual bool ContainsDataReductionProxy(
      const net::ProxyConfig::ProxyRules& proxy_rules);

 protected:
  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled| and |alternative_enabled|. Use the alternative
  // configuration only if |enabled| and |alternative_enabled| are true. If
  // |restricted| is true, only enable the fallback proxy. |at_startup| is true
  // when this method is called from InitDataReductionProxySettings.
  void SetProxyConfigOnIOThread(bool enabled,
                                bool alternative_enabled,
                                bool at_startup);

  // Writes a warning to the log that is used in backend processing of
  // customer feedback. Virtual so tests can mock it for verification.
  virtual void LogProxyState(bool enabled, bool restricted, bool at_startup);

  // Virtualized for mocking. Records UMA containing the result of requesting
  // the secure proxy check.
  virtual void RecordSecureProxyCheckFetchResult(
      SecureProxyCheckFetchResult result);

  // Virtualized for mocking. Returns the list of network interfaces in use.
  // |interfaces| can be null.
  virtual void GetNetworkList(net::NetworkInterfaceList* interfaces,
                              int policy);

 private:
  friend class DataReductionProxyConfigTest;
  friend class MockDataReductionProxyConfig;
  friend class TestDataReductionProxyConfig;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestSetProxyConfigsHoldback);

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Performs initialization on the IO thread.
  void InitOnIOThread();

  // Updates the Data Reduction Proxy configurator with the current config.
  virtual void UpdateConfigurator(bool enabled,
                                  bool alternative_enabled,
                                  bool restricted,
                                  bool at_startup);

  // Begins a secure proxy check to determine if the Data Reduction Proxy is
  // permitted to use the HTTPS proxy servers.
  void StartSecureProxyCheck();

  // Parses the secure proxy check responses and appropriately configures the
  // Data Reduction Proxy rules.
  virtual void HandleSecureProxyCheckResponse(
      const std::string& response, const net::URLRequestStatus& status);
  virtual void HandleSecureProxyCheckResponseOnIOThread(
      const std::string& response, const net::URLRequestStatus& status);

  // Adds the default proxy bypass rules for the Data Reduction Proxy.
  void AddDefaultProxyBypassRules();

  // Disables use of the Data Reduction Proxy on VPNs. Returns true if the
  // Data Reduction Proxy has been disabled.
  bool DisableIfVPN();

  bool restricted_by_carrier_;
  bool disabled_on_vpn_;
  bool unreachable_;
  bool enabled_by_user_;
  bool alternative_enabled_by_user_;

  // Contains the configuration data being used.
  scoped_ptr<DataReductionProxyParams> params_;

  // |io_task_runner_| should be the task runner for running operations on the
  // IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |ui_task_runner_| should be the task runner for running operations on the
  // UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // The caller must ensure that the |net_log_|, if set, outlives this instance.
  // It is used to create new instances of |bound_net_log_| on secure proxy
  // checks. |bound_net_log_| permits the correlation of the begin and end
  // phases of a given secure proxy check, and a new one is created for each
  // secure proxy check (with |net_log_| as a parameter).
  net::NetLog* net_log_;
  net::BoundNetLog bound_net_log_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // The caller must ensure that the |event_store_| outlives this instance.
  DataReductionProxyEventStore* event_store_;

  base::ThreadChecker thread_checker_;

  // A weak pointer to a |DataReductionProxyService| to perform secure proxy
  // checks. The weak pointer is required since the |DataReductionProxyService|
  // is destroyed before this instance of the |DataReductionProxyConfig|.
  base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
