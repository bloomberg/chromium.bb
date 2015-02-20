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

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
class URLRequestStatus;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyParams;
class DataReductionProxyService;

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

  // This method expects to run on the UI thread. It permits the data reduction
  // proxy configuration to change based on changes initiated by the user.
  virtual void SetProxyPrefs(bool enabled,
                             bool alternative_enabled,
                             bool at_startup);

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
  // the probe URL.
  virtual void RecordProbeURLFetchResult(ProbeURLFetchResult result);

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

  // Begins a probe request to determine if the Data Reduction Proxy is
  // permitted to use the HTTPS proxy servers.
  void StartProbe();

  // Parses the probe responses and appropriately configures the Data Reduction
  // Proxy rules.
  virtual void HandleProbeResponse(const std::string& response,
                                   const net::URLRequestStatus& status);
  virtual void HandleProbeResponseOnIOThread(
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
  // It is used to create new instances of |bound_net_log_| on canary
  // requests. |bound_net_log_| permits the correlation of the begin and end
  // phases of a given canary request, and a new one is created for each
  // canary check (with |net_log_| as a parameter).
  net::NetLog* net_log_;
  net::BoundNetLog bound_net_log_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // The caller must ensure that the |event_store_| outlives this instance.
  DataReductionProxyEventStore* event_store_;

  base::ThreadChecker thread_checker_;

  // A weak pointer to a |DataReductionProxyService| to perform probe requests.
  // The weak pointer is required since the |DataReductionProxyService| is
  // destroyed before this instance of the |DataReductionProxyConfig|.
  base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
