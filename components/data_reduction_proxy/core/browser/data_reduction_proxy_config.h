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
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
class URLFetcher;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyParams;

// Values of the UMA DataReductionProxy.ProbeURL histogram.
// This enum must remain synchronized with
// DataReductionProxyProbeURLFetchResult in metrics/histograms/histograms.xml.
// TODO(marq): Rename these histogram buckets with s/DISABLED/RESTRICTED/, so
//     their names match the behavior they track.
enum ProbeURLFetchResult {
  // The probe failed because the Internet was disconnected.
  INTERNET_DISCONNECTED = 0,

  // The probe failed for any other reason, and as a result, the proxy was
  // disabled.
  FAILED_PROXY_DISABLED,

  // The probe failed, but the proxy was already restricted.
  FAILED_PROXY_ALREADY_DISABLED,

  // The probe succeeded, and as a result the proxy was restricted.
  SUCCEEDED_PROXY_ENABLED,

  // The probe succeeded, but the proxy was already restricted.
  SUCCEEDED_PROXY_ALREADY_ENABLED,

  // This must always be last.
  PROBE_URL_FETCH_RESULT_COUNT
};

// Central point for holding the Data Reduction Proxy configuration.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyConfig
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // DataReductionProxyConfig will take ownership of |params|.
  DataReductionProxyConfig(scoped_ptr<DataReductionProxyParams> params);
  ~DataReductionProxyConfig() override;

  // Initializes the Data Reduction Proxy config with an |io_task_runner|,
  // |net_log|, a |UrlRequestContextGetter| for canary probes, a
  // |DataReductionProxyConfigurator| for setting the proxy configuration, and a
  // |DataReductionProxyEventStore| for logging event changes. The caller must
  // ensure that all parameters remain alive for the lifetime of the
  // |DataReductionProxyConfig| instance.
  void InitDataReductionProxyConfig(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      net::NetLog* net_log,
      net::URLRequestContextGetter* url_request_context_getter,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);

  // Returns the underlying |DataReductionProxyParams| instance.
  DataReductionProxyParams* params() const {
    return params_.get();
  }

 protected:
  // Virtualized for testing. Returns a fetcher for the probe to check if OK for
  // the proxy to use TLS.
  virtual net::URLFetcher* GetURLFetcherForProbe();

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled| and |alternative_enabled|. Use the alternative
  // configuration only if |enabled| and |alternative_enabled| are true. If
  // |restricted| is true, only enable the fallback proxy. |at_startup| is true
  // when this method is called from InitDataReductionProxySettings.
  virtual void SetProxyConfigs(bool enabled,
                               bool alternative_enabled,
                               bool restricted,
                               bool at_startup);

  // Writes a warning to the log that is used in backend processing of
  // customer feedback. Virtual so tests can mock it for verification.
  virtual void LogProxyState(bool enabled, bool restricted, bool at_startup);

  // Virtualized for mocking. Records UMA containing the result of requesting
  // the probe URL.
  virtual void RecordProbeURLFetchResult(ProbeURLFetchResult result);

  // Virtualized for mocking. Returns the list of network interfaces in use.
  // |interfaces| can be null.
  virtual void GetNetworkList(net::NetworkInterfaceList* interfaces,
                              int policy);

 private:
  friend class DataReductionProxyConfigTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, TestSetProxyConfigs);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestSetProxyConfigsHoldback);

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Adds the default proxy bypass rules for the Data Reduction Proxy.
  void AddDefaultProxyBypassRules();

  // Requests the proxy probe URL, if one is set.  If unable to do so, disables
  // the proxy, if enabled. Otherwise enables the proxy if disabled by a probe
  // failure.
  void ProbeWhetherDataReductionProxyIsAvailable();

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

  // The caller must ensure that the |net_log_|, if set, outlives this instance.
  // It is used to create new instances of |bound_net_log_| on canary
  // requests. |bound_net_log_| permits the correlation of the begin and end
  // phases of a given canary request, and a new one is created for each
  // canary check (with |net_log_| as a parameter).
  net::NetLog* net_log_;
  net::BoundNetLog bound_net_log_;

  // Used to retrieve a URLRequestContext for performing the canary check.
  net::URLRequestContextGetter* url_request_context_getter_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // The caller must ensure that the |event_store_| outlives this instance.
  DataReductionProxyEventStore* event_store_;

  base::ThreadChecker thread_checker_;

  // The URLFetcher being used for the canary check.
  scoped_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
