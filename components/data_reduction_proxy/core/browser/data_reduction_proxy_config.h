// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_retry_info.h"

class GURL;

namespace base {
class TimeDelta;
}

namespace net {
class HostPortPair;
class NetLog;
class URLFetcher;
class URLRequest;
class URLRequestContextGetter;
class URLRequestStatus;
}

namespace data_reduction_proxy {

typedef base::Callback<void(const std::string&,
                            const net::URLRequestStatus&,
                            int)> FetcherResponseCallback;

class DataReductionProxyConfigValues;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyService;
class SecureProxyChecker;
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

  // The secure proxy has been disabled on a network change until the check
  // succeeds.
  PROXY_DISABLED_BEFORE_CHECK,

  // This must always be last.
  SECURE_PROXY_CHECK_FETCH_RESULT_COUNT
};

// Auto LoFi current status.
enum AutoLoFiStatus {
  // Auto LoFi is either off or the current network conditions are not worse
  // than the ones specified in Auto LoFi field trial parameters.
  AUTO_LOFI_STATUS_DISABLED = 0,

  // Auto LoFi is off but the current network conditions are worse than the
  // ones specified in Auto LoFi field trial parameters.
  AUTO_LOFI_STATUS_OFF,

  // Auto LoFi is on and but the current network conditions are worse than the
  // ones specified in Auto LoFi field trial parameters.
  AUTO_LOFI_STATUS_ON,

  AUTO_LOFI_STATUS_LAST = AUTO_LOFI_STATUS_ON
};

// Central point for holding the Data Reduction Proxy configuration.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyConfig
    : public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime
  // of the |DataReductionProxyConfig| instance, with the exception of
  // |config_values| which is owned by |this|. |io_task_runner| is used to
  // validate calls on the correct thread. |event_creator| is used for logging
  // the start and end of a secure proxy check; |net_log| is used to create a
  // net::BoundNetLog for correlating the start and end of the check.
  // |config_values| contains the Data Reduction Proxy configuration values.
  // |configurator| is the target for a configuration update.
  DataReductionProxyConfig(
      net::NetLog* net_log,
      scoped_ptr<DataReductionProxyConfigValues> config_values,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator);
  ~DataReductionProxyConfig() override;

  // Performs initialization on the IO thread.
  void InitializeOnIOThread(const scoped_refptr<net::URLRequestContextGetter>&
                                url_request_context_getter);

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled| and |alternative_enabled|. Use the alternative
  // configuration only if |enabled| and |alternative_enabled| are true. If
  // |restricted| is true, only enable the fallback proxy. |at_startup| is true
  // when this method is called from InitDataReductionProxySettings.
  // TODO(jeremyim): Change enabled/alternative_enabled to be a single enum,
  // since there are only 3 valid states - also update in
  // DataReductionProxyIOData.
  void SetProxyConfig(bool enabled,
                      bool alternative_enabled,
                      bool at_startup);

  // Provides a mechanism for an external object to force |this| to refresh
  // the Data Reduction Proxy configuration from |config_values_| and apply to
  // |configurator_|. Used by the Data Reduction Proxy config service client.
  void ReloadConfig();

  // Returns true if a Data Reduction Proxy was used for the given |request|.
  // If true, |proxy_info.proxy_servers.front()| will contain the name of the
  // proxy that was used. Subsequent entries in |proxy_info.proxy_servers| will
  // contain the names of the Data Reduction Proxy servers that would be used if
  // |proxy_info.proxy_servers.front()| is bypassed, if any exist. In addition,
  // |proxy_info| will note if the proxy used was a fallback, an alternative,
  // or a proxy for ssl; these are not mutually exclusive. |proxy_info| can be
  // NULL if the caller isn't interested in its values.
  virtual bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if the specified |host_port_pair| matches a Data Reduction
  // Proxy. If true, |proxy_info.proxy_servers.front()| will contain the name of
  // the proxy that matches. Subsequent entries in |proxy_info.proxy_servers|
  // will contain the name of the Data Reduction Proxy servers that would be
  // used if |proxy_info.proxy_servers.front()| is bypassed, if any exist. In
  // addition, |proxy_info| will note if the proxy was a fallback, an
  // alternative, or a proxy for ssl; these are not mutually exclusive.
  // |proxy_info| can be NULL if the caller isn't interested in its values.
  // Virtual for testing.
  virtual bool IsDataReductionProxy(
      const net::HostPortPair& host_port_pair,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if this request would be bypassed by the Data Reduction Proxy
  // based on applying the |data_reduction_proxy_config| param rules to the
  // request URL.
  // Virtualized for mocking.
  virtual bool IsBypassedByDataReductionProxyLocalRules(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config) const;

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies that apply to the request scheme. If all possible data reduction
  // proxies are bypassed, returns the minimum retry delay of the bypassed data
  // reduction proxies in min_retry_delay (if not NULL). If there are no
  // bypassed data reduction proxies for the request scheme, returns false and
  // does not assign min_retry_delay.
  // Virtualized for mocking.
  virtual bool AreDataReductionProxiesBypassed(
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
      const net::ProxyConfig::ProxyRules& proxy_rules) const;

  // Returns true if the proxy was using the HTTP tunnel for a HTTPS origin.
  bool UsingHTTPTunnel(const net::HostPortPair& proxy_server) const;

  // Returns true if the Data Reduction Proxy configuration may be used.
  bool allowed() const;

  // Returns true if the alternative Data Reduction Proxy configuration may be
  // used.
  bool alternative_allowed() const;

  // Returns true if the Data Reduction Proxy promo may be shown. This is not
  // tied to whether the Data Reduction Proxy is enabled.
  bool promo_allowed() const;

  // Returns the Auto LoFi status. Enabling LoFi from command line switch has
  // no effect on Auto LoFi.
  AutoLoFiStatus GetAutoLoFiStatus() const;

 protected:
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
                           TestGetDataReductionProxies);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestOnIPAddressChanged_SecureProxyDisabledByDefault);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestSetProxyConfigsHoldback);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AreProxiesBypassed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AreProxiesBypassedRetryDelay);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestMaybeDisableIfVPNTrialDisabled);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestMaybeDisableIfVPNTrialEnabled);

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Updates the Data Reduction Proxy configurator with the current config.
  virtual void UpdateConfigurator(bool enabled,
                                  bool alternative_enabled,
                                  bool restricted,
                                  bool at_startup);

  // Requests the given |secure_proxy_check_url|. Upon completion, returns the
  // results to the caller via the |fetcher_callback|. Virtualized for unit
  // testing.
  virtual void SecureProxyCheck(const GURL& secure_proxy_check_url,
                                FetcherResponseCallback fetcher_callback);

  // Parses the secure proxy check responses and appropriately configures the
  // Data Reduction Proxy rules.
  void HandleSecureProxyCheckResponse(const std::string& response,
                                      const net::URLRequestStatus& status,
                                      int http_response_code);

  // Adds the default proxy bypass rules for the Data Reduction Proxy.
  void AddDefaultProxyBypassRules();

  // Disables use of the Data Reduction Proxy on VPNs if client is not in the
  // DataReductionProxyUseDataSaverOnVPN field trial and a TUN network interface
  // is being used. Returns true if the Data Reduction Proxy has been disabled.
  bool MaybeDisableIfVPN();

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies that apply to the request scheme. If all possible data reduction
  // proxies are bypassed, returns the minimum retry delay of the bypassed data
  // reduction proxies in min_retry_delay (if not NULL). If there are no
  // bypassed data reduction proxies for the request scheme, returns false and
  // does not assign min_retry_delay.
  bool AreProxiesBypassed(
      const net::ProxyRetryInfoMap& retry_map,
      const net::ProxyConfig::ProxyRules& proxy_rules,
      bool is_https,
      base::TimeDelta* min_retry_delay) const;

  // Returns true if this client is part of LoFi enabled field trial.
  // Virtualized for mocking.
  virtual bool IsIncludedInLoFiEnabledFieldTrial() const;

  // Returns true if this client is part of LoFi control field trial.
  // Virtualized for mocking.
  virtual bool IsIncludedInLoFiControlFieldTrial() const;

  // Returns true if current network conditions are worse than the ones
  // specified in the enabled or control field trial group parameters.
  // Virtualized for mocking.
  virtual bool IsNetworkBad() const;

  scoped_ptr<SecureProxyChecker> secure_proxy_checker_;

  // Indicates if the secure Data Reduction Proxy can be used or not.
  bool secure_proxy_allowed_;

  bool disabled_on_vpn_;
  bool unreachable_;
  bool enabled_by_user_;
  bool alternative_enabled_by_user_;

  // Contains the configuration data being used.
  scoped_ptr<DataReductionProxyConfigValues> config_values_;

  // The caller must ensure that the |net_log_|, if set, outlives this instance.
  // It is used to create new instances of |bound_net_log_| on secure proxy
  // checks. |bound_net_log_| permits the correlation of the begin and end
  // phases of a given secure proxy check, and a new one is created for each
  // secure proxy check (with |net_log_| as a parameter).
  net::NetLog* net_log_;
  net::BoundNetLog bound_net_log_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // The caller must ensure that the |event_creator_| outlives this instance.
  DataReductionProxyEventCreator* event_creator_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
