// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

namespace net {
class NetLog;
class URLRequestInterceptor;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyParams;
class DataReductionProxySettings;
class DataReductionProxyStatisticsPrefs;
class DataReductionProxyUsageStats;

// Contains and initializes all Data Reduction Proxy objects that operate on
// the IO thread.
class DataReductionProxyIOData {
 public:
  // Constructs a DataReductionProxyIOData object and takes ownership of
  // |params| and |statistics_prefs|. |params| contains information about the
  // DNS names used by the proxy, and allowable configurations.
  // |statistics_prefs| maintains compression statistics during use of the
  // proxy. |settings| provides a UI hook to signal when the proxy is
  // unavailable. Only |statistics_prefs.get()| may be null.
  DataReductionProxyIOData(
      const Client& client,
      scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs,
      DataReductionProxySettings* settings,
      net::NetLog* net_log,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  virtual ~DataReductionProxyIOData();

  // Initializes preferences, including a preference to track whether the
  // Data Reduction Proxy is enabled.
  void InitOnUIThread(PrefService* pref_service);

  // Destroys the statistics preferences.
  void ShutdownOnUIThread();

  // Constructs statistics prefs. This is not necessary if a valid statistics
  // prefs is passed into the constructor.
  void EnableCompressionStatisticsLogging(
      PrefService* prefs,
      const base::TimeDelta& commit_delay);

  // Creates an interceptor suitable for following the Data Reduction Proxy
  // bypass protocol.
  scoped_ptr<net::URLRequestInterceptor> CreateInterceptor();

  // Creates a NetworkDelegate suitable for carrying out the Data Reduction
  // Proxy protocol, including authenticating, establishing a handler to
  // override the current proxy configuration, and
  // gathering statistics for UMA.
  scoped_ptr<DataReductionProxyNetworkDelegate> CreateNetworkDelegate(
      scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
      bool track_proxy_bypass_statistics);

  // Returns true if the Data Reduction Proxy is enabled and false otherwise.
  bool IsEnabled() const;

  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  DataReductionProxyEventStore* event_store() const {
    return event_store_.get();
  }

  DataReductionProxyStatisticsPrefs* statistics_prefs() const {
    return statistics_prefs_.get();
  }

  net::ProxyDelegate* proxy_delegate() const {
    return proxy_delegate_.get();
  }

  net::NetLog* net_log() {
    return net_log_;
  }

  // Used for testing.
  DataReductionProxyUsageStats* usage_stats() const {
    return usage_stats_.get();
  }

 private:
  // The type of Data Reduction Proxy client.
  Client client_;

  // Parameters including DNS names and allowable configurations.
  scoped_ptr<DataReductionProxyParams> params_;

  // Tracker of compression statistics to be displayed to the user.
  scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs_;

  // Tracker of Data Reduction Proxy-related events, e.g., for logging.
  scoped_ptr<DataReductionProxyEventStore> event_store_;

  // Setter of the Data Reduction Proxy-specific proxy configuration.
  scoped_ptr<DataReductionProxyConfigurator> configurator_;

  // A proxy delegate. Used, for example, to add authentication to HTTP CONNECT
  // request.
  scoped_ptr<DataReductionProxyDelegate> proxy_delegate_;

  // User-facing settings object.
  DataReductionProxySettings* settings_;

  // Tracker of various metrics to be reported in UMA.
  scoped_ptr<DataReductionProxyUsageStats> usage_stats_;

  // Constructs credentials suitable for authenticating the client.
  scoped_ptr<DataReductionProxyAuthRequestHandler> auth_request_handler_;

  // A net log.
  net::NetLog* net_log_;

  // IO and UI task runners, respectively.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Used
  bool shutdown_on_ui_;

  // Preference that determines if the Data Reduction Proxy has been enabled
  // by the user. In practice, this can be overridden by the command line.
  BooleanPrefMember enabled_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyIOData);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

