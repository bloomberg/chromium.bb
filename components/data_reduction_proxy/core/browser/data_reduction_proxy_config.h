// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/secure_proxy_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/previews/core/previews_experiments.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_retry_info.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
class ProxyServer;
class URLRequest;
class URLRequestContextGetter;
class URLRequestStatus;
}

namespace previews {
class PreviewsDecider;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigValues;
class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class SecureProxyChecker;
class WarmupURLFetcher;
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

// Central point for holding the Data Reduction Proxy configuration.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyConfig
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime
  // of the |DataReductionProxyConfig| instance, with the exception of
  // |config_values| which is owned by |this|. |event_creator| is used for
  // logging the start and end of a secure proxy check; |net_log| is used to
  // create a net::NetLogWithSource for correlating the start and end of the
  // check. |config_values| contains the Data Reduction Proxy configuration
  // values. |configurator| is the target for a configuration update.
  DataReductionProxyConfig(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      net::NetLog* net_log,
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator);
  ~DataReductionProxyConfig() override;

  // Performs initialization on the IO thread.
  // |basic_url_request_context_getter| is the net::URLRequestContextGetter that
  // disables the use of alternative protocols and proxies.
  // |url_request_context_getter| is the default net::URLRequestContextGetter
  // used for making URL requests.
  void InitializeOnIOThread(const scoped_refptr<net::URLRequestContextGetter>&
                                basic_url_request_context_getter,
                            const scoped_refptr<net::URLRequestContextGetter>&
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
  // |proxy_info| will note if the proxy used was a fallback. |proxy_info| can
  // be NULL if the caller isn't interested in its values.
  virtual bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if the specified |proxy_server| matches a Data Reduction
  // Proxy. If true, |proxy_info.proxy_servers.front()| will contain the name of
  // the proxy that matches. Subsequent entries in |proxy_info.proxy_servers|
  // will contain the name of the Data Reduction Proxy servers that would be
  // used if |proxy_info.proxy_servers.front()| is bypassed, if any exist. In
  // addition, |proxy_info| will note if the proxy was a fallback. |proxy_info|
  // can be NULL if the caller isn't interested in its values. Virtual for
  // testing.
  virtual bool IsDataReductionProxy(
      const net::ProxyServer& proxy_server,
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

  // Returns true when Lo-Fi Previews should be activated. Records metrics for
  // Lo-Fi state changes. |request| is used to get the network quality estimator
  // from the URLRequestContext. |previews_decider| is used to check if
  // |request| is locally blacklisted.
  bool ShouldEnableLoFi(const net::URLRequest& request,
                        const previews::PreviewsDecider& previews_decider);

  // Returns true when Lite Page Previews should be activated. |request| is used
  // to get the network quality estimator from the URLRequestContext.
  // |previews_decider| is used to check if |request| is locally blacklisted.
  bool ShouldEnableLitePages(const net::URLRequest& request,
                             const previews::PreviewsDecider& previews_decider);

  // Returns true if the data saver has been enabled by the user, and the data
  // saver proxy is reachable.
  bool enabled_by_user_and_reachable() const;

  // Gets the ProxyConfig that would be used ignoring the holdback experiment.
  // This should only be used for logging purposes.
  net::ProxyConfig ProxyConfigIgnoringHoldback() const;

  bool secure_proxy_allowed() const;

  std::vector<DataReductionProxyServer> GetProxiesForHttp() const;

 protected:
  virtual base::TimeTicks GetTicksNow() const;

  // Updates the Data Reduction Proxy configurator with the current config.
  void UpdateConfigForTesting(bool enabled, bool restricted);

 private:
  friend class MockDataReductionProxyConfig;
  friend class TestDataReductionProxyConfig;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestSetProxyConfigsHoldback);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AreProxiesBypassed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AreProxiesBypassedRetryDelay);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, WarmupURL);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           ShouldAcceptServerLoFi);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           ShouldAcceptServerPreview);

  // Values of the estimated network quality at the beginning of the most
  // recent query of the Network Quality Estimator.
  enum NetworkQualityAtLastQuery {
    NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN,
    NETWORK_QUALITY_AT_LAST_QUERY_SLOW,
    NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW
  };

  // NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Requests the secure proxy check URL. Upon completion, returns the results
  // to the caller via the |fetcher_callback|. Virtualized for unit testing.
  virtual void SecureProxyCheck(SecureProxyCheckerCallback fetcher_callback);

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

  // Returns whether the request is blacklisted (or if Lo-Fi is disabled).
  bool IsBlackListedOrDisabled(
      const net::URLRequest& request,
      const previews::PreviewsDecider& previews_decider,
      previews::PreviewsType previews_type) const;

  // Returns whether the client should report to the data reduction proxy that
  // it is willing to accept server previews for |request|.
  // |previews_decider| is used to check if |request| is locally blacklisted.
  // Should only be used if the kDataReductionProxyDecidesTransform feature is
  // enabled.
  bool ShouldAcceptServerPreview(
      const net::URLRequest& request,
      const previews::PreviewsDecider& previews_decider) const;

  // Checks if the current network has captive portal, and handles the result.
  // If the captive portal probe was blocked on the current network, disables
  // the use of secure proxies.
  void HandleCaptivePortal();

  // Returns true if the current network has captive portal. Virtualized
  // for testing.
  virtual bool GetIsCaptivePortal() const;

  // Fetches the warmup URL.
  void FetchWarmupURL();

  // URL fetcher used for performing the secure proxy check.
  std::unique_ptr<SecureProxyChecker> secure_proxy_checker_;

  // URL fetcher used for fetching the warmup URL.
  std::unique_ptr<WarmupURLFetcher> warmup_url_fetcher_;

  // Indicates if the secure Data Reduction Proxy can be used or not.
  bool secure_proxy_allowed_;

  bool unreachable_;
  bool enabled_by_user_;

  // Contains the configuration data being used.
  std::unique_ptr<DataReductionProxyConfigValues> config_values_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The caller must ensure that the |net_log_|, if set, outlives this instance.
  // It is used to create new instances of |net_log_with_source_| on secure
  // proxy checks. |net_log_with_source_| permits the correlation of the begin
  // and end phases of a given secure proxy check, and a new one is created for
  // each secure proxy check (with |net_log_| as a parameter).
  net::NetLog* net_log_;
  net::NetLogWithSource net_log_with_source_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // The caller must ensure that the |event_creator_| outlives this instance.
  DataReductionProxyEventCreator* event_creator_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  // The current connection type.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // Set to true if the captive portal probe for the current network has been
  // blocked.
  bool is_captive_portal_;

  base::WeakPtrFactory<DataReductionProxyConfig> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
