// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_retry_info.h"

class GURL;

namespace net {
class HostPortPair;
class NetLog;
class NetworkQualityEstimator;
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

// Values of the |lofi_status_|.
// Default state is |LOFI_STATUS_TEMPORARILY_OFF|.
enum LoFiStatus {
  // Used if Lo-Fi is permanently off.
  LOFI_STATUS_OFF = 0,

  // Used if Lo-Fi is disabled temporarily through direct or indirect user
  // action. The state would be reset on next main frame request.
  LOFI_STATUS_TEMPORARILY_OFF,

  // Used if Lo-Fi is enabled through flags.
  LOFI_STATUS_ACTIVE_FROM_FLAGS,

  // Session is in Auto Lo-Fi Control group and the current conditions are
  // suitable to use Lo-Fi "q=low" header.
  LOFI_STATUS_ACTIVE_CONTROL,

  // Session is in Auto Lo-Fi Control group and the current conditions are
  // not suitable to use Lo-Fi "q=low" header.
  LOFI_STATUS_INACTIVE_CONTROL,

  // Session is in Auto Lo-Fi enabled group and the current conditions are
  // suitable to use Lo-Fi "q=low" header.
  LOFI_STATUS_ACTIVE,

  // Session is in Auto Lo-Fi enabled group and the current conditions are
  // not suitable to use Lo-Fi "q=low" header.
  LOFI_STATUS_INACTIVE,

  LOFI_STATUS_LAST = LOFI_STATUS_INACTIVE
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
  // the value of |enabled|. If |restricted| is true, only enable the fallback
  // proxy. |at_startup| is true when this method is called from
  // InitDataReductionProxySettings.
  void SetProxyConfig(bool enabled, bool at_startup);

  // Provides a mechanism for an external object to force |this| to refresh
  // the Data Reduction Proxy configuration from |config_values_| and apply to
  // |configurator_|. Used by the Data Reduction Proxy config service client.
  void ReloadConfig();

  // Returns true if a Data Reduction Proxy was used for the given |request|.
  // If true, |proxy_info.proxy_servers.front()| will contain the name of the
  // proxy that was used. Subsequent entries in |proxy_info.proxy_servers| will
  // contain the names of the Data Reduction Proxy servers that would be used if
  // |proxy_info.proxy_servers.front()| is bypassed, if any exist. In addition,
  // |proxy_info| will note if the proxy used was a fallback, or a proxy for
  // ssl; these are not mutually exclusive. |proxy_info| can be NULL if the
  // caller isn't interested in its values.
  virtual bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if the specified |host_port_pair| matches a Data Reduction
  // Proxy. If true, |proxy_info.proxy_servers.front()| will contain the name of
  // the proxy that matches. Subsequent entries in |proxy_info.proxy_servers|
  // will contain the name of the Data Reduction Proxy servers that would be
  // used if |proxy_info.proxy_servers.front()| is bypassed, if any exist. In
  // addition, |proxy_info| will note if the proxy was a fallback or a proxy for
  // ssl; these are not mutually exclusive. |proxy_info| can be NULL if the
  // caller isn't interested in its values.
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

  // Returns true if the Data Reduction Proxy promo may be shown. This is not
  // tied to whether the Data Reduction Proxy is enabled.
  bool promo_allowed() const;

  // Returns the Lo-Fi status.
  LoFiStatus GetLoFiStatus() const;

  // Returns true only if Lo-Fi "q=low" header should be added to the Chrome
  // Proxy header.
  // Should be called on all URL requests (main frame and non main frame).
  bool ShouldUseLoFiHeaderForRequests() const;

  // Returns true if the session is in the Lo-Fi control experiment. This
  // happens if user is in Control group, and connection is slow.
  bool IsInLoFiActiveControlExperiment() const;

  // Sets |lofi_status_| to LOFI_STATUS_OFF.
  void SetLoFiModeOff();

  // Updates |lofi_status_| based on the arguments provided and the current
  // value of |lofi_status_|.
  // |network_quality_estimator| may be NULL.
  // Should be called only on main frame loads.
  void UpdateLoFiStatusOnMainFrameRequest(
      bool user_temporarily_disabled_lofi,
      const net::NetworkQualityEstimator* network_quality_estimator);

 protected:
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
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, AutoLoFiParams);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AutoLoFiParamsSlowConnectionsFlag);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, AutoLoFiAccuracy);

  // Values of the estimated network quality at the beginning of the most
  // recent main frame request.
  enum NetworkQualityAtLastMainFrameRequest {
    NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_UNKNOWN,
    NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_SLOW,
    NETWORK_QUALITY_AT_LAST_MAIN_FRAME_REQUEST_NOT_SLOW
  };

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Updates the Data Reduction Proxy configurator with the current config.
  virtual void UpdateConfigurator(bool enabled, bool restricted);

  // Populates the parameters for the Lo-Fi field trial if the session is part
  // of either Lo-Fi enabled or Lo-Fi control field trial group.
  void PopulateAutoLoFiParams();

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

  // Returns true if this client is part of Lo-Fi enabled field trial.
  // Virtualized for unit testing.
  virtual bool IsIncludedInLoFiEnabledFieldTrial() const;

  // Returns true if this client is part of Lo-Fi control field trial.
  // Virtualized for unit testing.
  virtual bool IsIncludedInLoFiControlFieldTrial() const;

  // Returns true if expected throughput is lower than the one specified in the
  // Auto Lo-Fi field trial parameters OR if the expected round trip time is
  // higher than the one specified in the Auto Lo-Fi field trial parameters.
  // |network_quality_estimator| may be NULL.
  // Virtualized for unit testing. Should be called only on main frame loads.
  virtual bool IsNetworkQualityProhibitivelySlow(
      const net::NetworkQualityEstimator* network_quality_estimator);

  // Returns true only if Lo-Fi "q=low" header should be added to the Chrome
  // Proxy header based on the value of |lofi_status|.
  static bool ShouldUseLoFiHeaderForRequests(LoFiStatus lofi_status);

  // Records Lo-Fi accuracy metric. Should be called only on main frame loads.
  void RecordAutoLoFiAccuracyRate(
      const net::NetworkQualityEstimator* network_quality_estimator) const;

  scoped_ptr<SecureProxyChecker> secure_proxy_checker_;

  // Indicates if the secure Data Reduction Proxy can be used or not.
  bool secure_proxy_allowed_;

  bool unreachable_;
  bool enabled_by_user_;

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

  // Thresholds from the field trial at which auto Lo-Fi is turned on.
  // If the expected round trip time is higher than |auto_lofi_minimum_rtt_|,
  // auto Lo-Fi would be turned on.
  base::TimeDelta auto_lofi_minimum_rtt_;

  // If the expected throughput in Kbps is lower than
  // |auto_lofi_maximum_kbps_|, auto Lo-Fi would be turned on.
  int32_t auto_lofi_maximum_kbps_;

  // State of auto Lo-Fi is not changed more than once in any period of
  // duration shorter than |auto_lofi_hysteresis_|.
  base::TimeDelta auto_lofi_hysteresis_;

  // Time when the network quality was last updated.
  base::TimeTicks network_quality_last_updated_;

  // True iff the network was determined to be prohibitively slow when the
  // network quality was last updated. This happens on main frame request, and
  // not more than once in any window of duration shorter than
  // |auto_lofi_hysteresis_|.
  bool network_prohibitively_slow_;

  // Set to the connection type reported by NetworkChangeNotifier when the
  // network quality was last updated (most recent main frame request).
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // Current Lo-Fi status.
  // The value changes only on main frame load.
  LoFiStatus lofi_status_;

  // Timestamp when the most recent main frame request started.
  base::TimeTicks last_main_frame_request_;

  // Holds the estimated network quality at the beginning of the most recent
  // main frame request. This should be used only for the purpose of recording
  // Lo-Fi accuracy UMA.
  NetworkQualityAtLastMainFrameRequest
      network_quality_at_last_main_frame_request_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
